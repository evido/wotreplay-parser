/*
 Copyright (c) 2012, Jan Temmerman
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the <organization> nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL Jan Temmerman BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "image.h"
#include "json/json.h"
#include "parser.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <boost/multi_array.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <tuple>
#include <tbb/tbb.h>
#include <vector>

using namespace std;
using namespace wotreplay;
using namespace tbb;
using namespace boost::filesystem;
using namespace boost;

/**
 * Collection of values used and filled in during the processing of the replay
 * file, an instance of this object is passed allong the pipeline.
 */
struct process_result {
    /** Indicates if an error has occured somewhere in the pipeline. */
    bool error;
    /** The path for the replay file to proces. */
    std::string path;
    /** An instance of the parser for the member path. */
    game_t *replay;
    /** images for both teams containing the number of times this position was filled by a team member. */
    boost::multi_array<float, 3> position_image;
    /** images for both teams containing the number of team member was killed on a specific position. */
    boost::multi_array<float, 3> death_image;
    /** images for both teams containing the number of times as team member was hit. */
    boost::multi_array<float, 3> hit_image;
    /**
     * Constructor for process_result, setting the path and the image sizes.
     */
    process_result(std::string path, int image_width, int image_height)
        : error(false), path(path), replay(nullptr)
        , position_image(boost::extents[2][image_width][image_height])
        , death_image(boost::extents[2][image_width][image_height])
        , hit_image(boost::extents[2][image_width][image_height])
    {
        
    }
    /**
     * No copy constructor available.
     */
    process_result(const process_result&) = delete;
    /**
     * No assignation possible.
     */
    process_result & operator= (const process_result & other) = delete;
    /** 
     * Destructor for the class process_result.
     */
    ~process_result() {
        delete replay;
    }
};

std::tuple<float, float> get_bounds(boost::multi_array<float, 3>::const_reference image, float l_quant,float r_quant) {
    std::vector<float> values;
    auto not_zero = [](float value) { return value != 0.f; };
    std::copy_if(image.origin(), image.origin() + image.num_elements(), std::inserter(values, values.begin()), not_zero);
    std::nth_element(values.begin(), values.begin() + static_cast<int>(l_quant*values.size()), values.end());
    float lower_bound = *(values.begin() + static_cast<int>(l_quant*values.size()));
    std::nth_element(values.begin(), values.begin() + static_cast<int>(r_quant*values.size()), values.end());
    float upper_bound = *(values.begin() + static_cast<int>(r_quant*values.size()));
    return std::make_tuple(lower_bound, upper_bound);
}

float calculate_alpha(const std::tuple<float, float> &bounds, float value) {
    float nominator = value - std::get<0>(bounds);
    float denominator = std::get<1>(bounds) - std::get<0>(bounds);
    return std::min(std::max( nominator /  denominator, 0.f), 1.f);
}

void draw_image(boost::multi_array<uint8_t, 3> &base,
                boost::multi_array<float, 3>::const_reference team1,
                boost::multi_array<float, 3>::const_reference team2,
                float l_quant, float u_quant) {
    
    std::array<std::tuple<float, float>, 2> bounds = {{
        get_bounds(team1, l_quant, u_quant),
        get_bounds(team2, l_quant, u_quant)
    }};

    auto shape = base.shape();
    for (size_t i = 0; i < shape[0]; ++i) {
        for (size_t j = 0; j < shape[1]; ++j) {
            float a[2]  = {
                calculate_alpha(bounds[0], team1[i][j]),
                calculate_alpha(bounds[1], team2[i][j])
            };
            base[i][j][0] = mix(base[i][j][0], 0, a[0], 255, a[1]);
            base[i][j][1] = mix(base[i][j][1], 255, a[0], 0, a[1]);
            base[i][j][2] = mix(base[i][j][2], 0, a[0], 0, a[1]);
        }
    }
};

std::function<void(const blocked_range<size_t>&)> merge_image(boost::multi_array<float,3>::reference left,
                                                        boost::multi_array<float,3>::reference right) {
    return [=]( const blocked_range<size_t>& range )  {
        boost::multi_array<float,3>::reference l(left);
        boost::multi_array<float,3>::reference r(right);
        for (size_t i = range.begin(); i != range.end(); ++i) {
            l.origin()[i] += r.origin()[i];
        }
    };
};

void draw_position(const packet_t &packet, const game_t &game, boost::multi_array<float,3> &images) {
    uint32_t player_id = packet.player_id();

    int team_id = game.get_team_id(player_id);
    if (team_id < 0) return;

    auto shape = images.shape();
    int width = static_cast<int>(shape[2]);
    int height = static_cast<int>(shape[1]);

    float x,y;
    std::tie(x,y) = get_2d_coord( packet.position(), game, width, height);

    if (x >= 0 && y >= 0 && x <= (width - 1) && y <= (height - 1)) {
        float px = x - floor(x), py = y - floor(y);
        images[team_id][floor(y)][floor(x)] += px*py;
        images[team_id][ceil(y)][floor(x)] += px*(1-py);
        images[team_id][floor(y)][ceil(x)] += (1-px)*py;
        images[team_id][ceil(y)][ceil(x)] += (1-px)*(1-py);
    }
}

void draw_death(const packet_t &packet, process_result &result) {
    uint32_t killer, killed;
    std::tie(killed, killer) = packet.tank_destroyed();
    packet_t position_packet;
    bool found = result.replay->find_property(packet.clock(), killed, property_t::position, position_packet);
    if (found) {
        draw_position(position_packet, *result.replay, result.death_image);
    }
}

void process_replay_directory(const path& directory) {
    directory_iterator it(directory);
    // get out if no elements
    if (it == directory_iterator()) return;

    std::atomic<int> count(0);
    auto f_generate_paths = [&it, &count](tbb::flow_control &fc) -> process_result* {
        std::string file_path;
        while (++it != directory_iterator() && !is_regular_file(*it));
        if (it == directory_iterator()) {
            fc.stop();
        } else {
            auto path = it->path();
            file_path = path.string();
            ++count;
        }
        return new process_result{ file_path, 500, 500 };
    };

    auto f_create_replays = [](process_result* result) -> process_result* {
        if (result == nullptr) {
            result->error = true;
            return result;
        }
        try {
            ifstream is(result->path, std::ios::binary);
            parser_t parser;
            result->replay = new game_t();
            parser.parse(is, *result->replay);
            is.close();
        } catch (std::exception &e) {
            result->error = true;
            std::cerr << "Error!" << std::endl;
            result->replay = nullptr;
        }
        return result;
    };

    auto f_process_replays = [](process_result *result) -> process_result* {
        if (result->error) return result;

        const game_t &replay = *result->replay;
        std::set<uint32_t> dead_players;

        for (const packet_t &packet : replay.get_packets()) {
            if (packet.has_property(property_t::tank_destroyed)) {
                draw_death(packet, *result);
                uint32_t killer, killed;
                std::tie(killed, killer) = packet.tank_destroyed();
                dead_players.insert(killed);
            }
            
            if (packet.has_property(property_t::position)
                && packet.has_property(property_t::player_id)
                && dead_players.find(packet.player_id()) == dead_players.end()) {
                draw_position(packet, replay, result->position_image);
            }
            
        }
        return result;
    };

    auto f_create_image = [](process_result* result) -> process_result* {
        if (result->error) return result;
        boost::multi_array<uint8_t, 3> base(boost::extents[500][500][4]);

        const game_t &replay = *result->replay;
        
        read_mini_map(replay.get_map_name(), replay.get_game_mode(), base);

        for (int i = 0; i < 500; ++i) {
            for (int j = 0; j < 500; ++j) {
                if (result->position_image[0][i][j] > 0) {
                    base[i][j][0] = 0x00;
                    base[i][j][1] = 0xFF;
                    base[i][j][2] = 0x00;
                    base[i][j][3] = 0xFF;
                }
                if (result->position_image[1][i][j] > 0) {
                    base[i][j][1] = 0x00;
                    base[i][j][0] = 0xFF;
                    base[i][j][2] = 0x00;
                    base[i][j][3] = 0xFF;
                }
            }
        }

        std::stringstream death_image;
        death_image << "out/deaths/" << replay.get_map_name() << "_" << replay.get_game_mode() << ".png";
        write_image(death_image.str(), base);
        return result;
    };
    
    std::map<std::tuple<std::string, std::string>, boost::multi_array<float,3>> images;
    auto f_merge_results = [&](process_result *result) -> process_result* {
        if (result->error /* || result->replay->get_game_end().size() == 0 */ ) return result;

        const game_t &replay = *result->replay;

        auto key = std::make_tuple(replay.get_map_name(), replay.get_game_mode());
        if (images.find(key) == images.end()) {
            images.insert({key, boost::multi_array<float,3>(boost::extents[4][500][500])});
        }

        boost::multi_array<float,3> &image = images[key];

        auto shape = image[0].shape();
        auto size = (shape[0])*(shape[1]);
        parallel_for(blocked_range<size_t>(0, size), merge_image(image[0], result->position_image[0]), auto_partitioner());
        parallel_for(blocked_range<size_t>(0, size), merge_image(image[1], result->position_image[1]), auto_partitioner());
        parallel_for(blocked_range<size_t>(0, size), merge_image(image[2], result->death_image[0]), auto_partitioner());
        parallel_for(blocked_range<size_t>(0, size), merge_image(image[3], result->death_image[1]), auto_partitioner());

        return result;
    };

    auto f_clean_up = [](process_result* result) -> void {
        delete result;
    };

    tbb::parallel_pipeline(10,
                           tbb::make_filter<void, process_result*>(tbb::filter::serial_in_order, f_generate_paths) &
                           tbb::make_filter<process_result*, process_result*>(tbb::filter::parallel, f_create_replays) &
                           tbb::make_filter<process_result*, process_result*>(tbb::filter::parallel, f_process_replays) &
//                           tbb::make_filter<process_result*, process_result*>(tbb::filter::parallel, f_create_image) &
                           tbb::make_filter<process_result*, process_result*>(tbb::filter::serial_out_of_order, f_merge_results) &
                           tbb::make_filter<process_result*, void>(tbb::filter::parallel, f_clean_up));

    typedef std::map<std::tuple<std::string, std::string>, boost::multi_array<float, 3>>::const_reference images_entry;
    tbb::parallel_do(images.begin(), images.end(), [](images_entry entry){
        const boost::multi_array<float, 3> &image = entry.second;

                std::string map_name, game_mode;
        std::tie(map_name, game_mode) = entry.first;
        boost::multi_array<uint8_t, 3> base;

        // process positions
        read_mini_map(map_name, game_mode, base);
        draw_image(base, image[0], image[1], 0.02, 0.98);
        std::stringstream position_image;
        position_image << "out/positions/" << map_name << "_" << game_mode << ".png";
        write_image(position_image.str(), base);

        // process deaths
        read_mini_map(map_name, game_mode, base);
        draw_image(base, image[2], image[3], .33, .66);
        std::stringstream death_image;
        death_image << "out/deaths/" << map_name << "_" << game_mode << ".png";
        write_image(death_image.str(), base);
    });
}



int main(int argc, const char * argv[]) {
    chdir("/Users/jantemmerman/Development/wotreplay-parser/data");
    
    // validate_parser("replays"); std::exit(0);
    // process_replay_directory("replays"); std::exit(1);
    
    string file_names[] = {
        "replays/20120407_1046_ussr-KV-3_himmelsdorf.wotreplay",
        "replays/8.0/20120929_1204_ussr-IS-3_28_desert.wotreplay",
        "replays/old/20120317_2037_ussr-KV-3_lakeville.wotreplay",
        "replays/20120610_1507_germany-E-75_caucasus.wotreplay",
        "replays/20120408_2137_ussr-KV-3_ruinberg.wotreplay",
        "replays/20120407_1322_ussr-KV_fjord.wotreplay",
        "replays/20120407_1046_ussr-KV-3_himmelsdorf.wotreplay",
        "replays/20120405_2122_germany-PzVIB_Tiger_II_redshire.wotreplay",
        "replays/20120405_2112_germany-PzVIB_Tiger_II_monastery.wotreplay",
        "replays/20120405_2204_ussr-KV_caucasus.wotreplay",
        "replays/20120707_2059_germany-E-75_himmelsdorf.wotreplay",
        "replays/20120815_0309_germany-E-75_02_malinovka.wotreplay",
        "replays/8.0/20120906_2352_germany-Panther_II_02_malinovka.wotreplay",
        "replays/20120826_0013_france-AMX_13_90_04_himmelsdorf.wotreplay",
        "replays/20120826_0019_france-AMX_13_90_45_north_america.wotreplay",
        "replays/20120701_1247_germany-E-75_monastery.wotreplay",
        "replays/20120826_2059_france-AMX_13_90_02_malinovka.wotreplay",
        "replays/20120826_1729_france-AMX_13_90_04_himmelsdorf.wotreplay",
        "replays/20120920_2130_france-AMX_13_90_14_siegfried_line.wotreplay",
        "replays/20120921_0042_ussr-IS-3_02_malinovka.wotreplay",
        "replays/old/20120319_2306_ussr-KV-3_malinovka.wotreplay",
        "replays/old/20120318_0044_germany-PzVIB_Tiger_II_himmelsdorf.wotreplay",
        "replays/old/20120317_2037_ussr-KV-3_lakeville.wotreplay",
        "replays/8.0/20120929_1724_ussr-IS-3_17_munchen.wotreplay",
        
    };

    auto file_name = file_names[11];
    ifstream is(file_name, std::ios::binary);

    if (!is) {
        std::cerr << "Something went wrong with reading file: " << file_name << std::endl;
        std::exit(-1);
    }
    
    parser_t parser;
    game_t game;
    parser.parse(is, game);
    is.close();

    // display some info about the replay
    show_packet_summary(game.get_packets());
    write_parts_to_file(game);
    show_map_boundaries(game, game.get_packets());
    write_parts_to_file(game);

    // create image
    // boost::multi_array<uint8_t, 3> image;
    // read_mini_map(game.get_map_name(), game.get_game_mode(), image);
    // create_image(image, game);
    // write_image("out/replay.png", image);
    image_t image(game.get_map_name(), game.get_game_mode());
    image.update(game);
    image.finish();
    image.write_image("test.png");
    
    return EXIT_SUCCESS;
}
