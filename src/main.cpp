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

#include "parser.h"
#include <png.h>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <algorithm>
#include <map>
#include <cmath>
#include <set>
#include <vector>
#include <array>
#include "json/json.h"
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <tbb/tbb.h>
#include <regex>
#include <boost/multi_array.hpp>


// generates unavoidable unused variable warnings
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

#include <boost/accumulators/numeric/functional/vector.hpp>
#include <boost/accumulators/numeric/functional/complex.hpp>
#include <boost/accumulators/numeric/functional/valarray.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/tail_quantile.hpp>

#pragma clang diagnostic pop

#include <atomic>

/** @file */

using namespace std;
using namespace wotreplay;
using namespace tbb;
using namespace boost::filesystem;
using namespace boost::accumulators;
using namespace boost;
using boost::accumulators::left;
using boost::accumulators::right;

std::size_t c =  250000; // cache size

typedef accumulator_set<double, stats<tag::tail_quantile<boost::accumulators::right>>> accumulator_t_right;
typedef accumulator_set<double, stats<tag::tail_quantile<boost::accumulators::left>>> accumulator_t_left;




/**
 * @fn std::tuple<float, float> get_2d_coord(const std::tuple<float, float, float> &position, const game_info &game_info, int width, int height)
 * @brief Get 2D Coordinates from a WOT position scaled to given width and height.
 * @param position The position to convert to a 2d coordinate
 * @param game_info game_info object containing the boundaries of the map
 * @param width target map width to scale the coordinates too
 * @param height target map height to scale the coordinates too
 * @return The scaled 2d coordinates
 */
std::tuple<float, float> get_2d_coord(const std::tuple<float, float, float> &position, const game_info &game_info, int width, int height) {
    float x,y,z, min_x, max_x, min_y, max_y;
    std::tie(x,z,y) = position;
    std::tie(min_x, max_x) = game_info.boundaries[0];
    std::tie(min_y, max_y) = game_info.boundaries[1];
    x = (x - min_x) * (width / (max_x - min_x + 1));
    y = (max_y - y) * (height / (max_y - min_y + 1));
    return std::make_tuple(x,y);
}

/**
 * @fn int get_team(const game_info &game_info, uint32_t player_id)
 * @brief Determine team for a player.
 * @param player_id The player_id for which to determine the team.
 * @param game_info The game info containing the seperate teams.
 * @return The team-id for the given player-id.
 */
int get_team(const game_info &game_info, uint32_t player_id) {
    const auto &teams = game_info.teams;
    auto it = std::find_if(teams.begin(), teams.end(), [&](const std::set<int> &team) {
        return team.find(player_id) != team.end();
    });
    return it == teams.end() ? -1 : (static_cast<int>(it - teams.begin()));
}

/**
 * @fn void create_image(boost::multi_array<uint8_t, 3> &image, const parser &replay)
 * @brief Draw an a 500x500 image (with alpha channel) the positions of the tanks, and the death positions.
 * @param image The target image to draw on.
 * @param replay The replay with the information to draw
 */
void create_image(boost::multi_array<uint8_t, 3> &image, const parser &replay) {
    const std::vector<packet_t> &packets = replay.get_packets();
    const game_info &game_info = replay.get_game_info();

    auto shape = image.shape();
    int width = static_cast<int>(shape[1]);
    int height = static_cast<int>(shape[0]);

    for (const packet_t &packet : packets) {

        if (packet.has_property(property::position)) {
            uint32_t player_id = packet.player_id();

            float f_x,f_y;
            auto position = packet.position();
            std::tie(f_x,f_y) = get_2d_coord(position, game_info, width, height);
            int x = std::round(f_x);
            int y = std::round(f_y);

            if (player_id == game_info.recorder_id) {
                image[y][x][2]= 0xff;
                image[y][x][1] = image[y][x][0] = 0x00;
                image[y][x][3] = 0xff;
                continue;
            }

            int team_id = get_team(game_info, player_id);
            if (team_id < 0) continue;

            
            if (__builtin_expect(f_x >= 0 && f_y >= 0 && f_x <= (width - 1) && f_y <= (height - 1), 1)) {
                switch (team_id) {
                    case 0:
                        image[y][x][0] = image[y][x][2] = 0x00;
                        image[y][x][1] = 0xff;
                        image[y][x][3] = 0xff;
                        break;
                    case 1:
                        image[y][x][0]= 0xff;
                        image[y][x][1] = image[y][x][2] = 0x00;
                        image[y][x][3] = 0xff;
                        break;
                }
            }
        }

        if (packet.has_property(property::tank_destroyed)) {
            uint32_t target, killer;
            std::tie(target, killer) = packet.tank_destroyed();
            packet_t position_packet;
            bool found = replay.find_property(packet.clock(), target, property::position, position_packet);
            if (found) {
                auto position = position_packet.position();
                static int offsets[][2] = {
                    {-1, -1},
                    {-1,  0},
                    {-1,  1},
                    { 0,  1},
                    { 1,  1},
                    { 1,  0},
                    { 1, -1},
                    { 0, -1},
                    { 0,  0}
                };
                for (auto offset : offsets) {
                    float f_x, f_y;
                    std::tie(f_x,f_y) = get_2d_coord(position, game_info, width, height);
                    int x = std::round(f_x) + offset[0];
                    int y = std::round(f_y) + offset[1];
                    image[y][x][3] = 0xff;
                    image[y][x][0] = image[y][x][1] = 0xFF;
                    image[y][x][2] = 0x00;
                }
            }
        }
    }
}


bool write_png(const std::string &out, png_bytepp image, size_t width, size_t height, bool alpha) {
    FILE *fp = fopen(out.c_str(), "wb");

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_filter(png_ptr, 0,PNG_FILTER_VALUE_NONE);
    png_set_IHDR(png_ptr, info_ptr, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                 8, alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_rows(png_ptr, info_ptr, image);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    
    return true;
}

void read_png(const std::string &in, png_bytepp image, int &width, int &height, int &channels) {
    FILE *fp = fopen(in.c_str(), "rb");
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_infop end_info = png_create_info_struct(png_ptr);
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return;
    }
    png_init_io(png_ptr, fp);
    png_set_rows(png_ptr, info_ptr, image);
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);
    height = png_get_image_height(png_ptr, info_ptr);
    width = png_get_image_width(png_ptr, info_ptr);
    channels = png_get_channels(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(fp);
}



template <typename T>
using image_t = boost::multi_array<T, 2>;

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
    parser *replay;
    /** images for both teams containing the number of times this position was filled by a team member. */
    std::array<image_t<float>, 2> position_image;
    /** images for both teams containing the number of team member was killed on a specific position. */
    std::array<image_t<float>, 2> death_image;
    /** images for both teams containing the number of times as team member was hit. */
    std::array<image_t<float>, 2> hit_image;
    /**
     * Constructor for process_result, setting the path and the image sizes.
     */
    process_result(std::string path, int image_width, int image_height)
        : path(path), position_image({
            image_t<float>(boost::extents[image_width][image_height]),
            image_t<float>(boost::extents[image_width][image_height])
        }), death_image({
            image_t<float>(boost::extents[image_width][image_height]),
            image_t<float>(boost::extents[image_width][image_height])
        }), hit_image({
            image_t<float>(boost::extents[image_width][image_height]),
            image_t<float>(boost::extents[image_width][image_height])
        }), replay(nullptr), error(false)
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



void get_row_pointers(multi_array<uint8_t, 3> &image, std::vector<png_bytep> &row_pointers) {
    auto shape = image.shape();
    size_t rows = shape[0], column_bytes = shape[1]*shape[2];
    for (int i = 0; i < rows; ++i) {
        row_pointers.push_back(image.data() + i*column_bytes);
    }
};

void write_image(const std::string &file_name, boost::multi_array<uint8_t, 3> &image) {
    auto shape = image.shape();
    std::vector<png_bytep> row_pointers;
    get_row_pointers(image, row_pointers);
    write_png(file_name, &row_pointers[0], shape[0], shape[1], shape[2] == 4);
};

void read_mini_map(const std::string &map_name, const std::string &game_mode, boost::multi_array<uint8_t, 3> &base) {
    base.resize(boost::extents[500][500][4]);
    std::vector<png_bytep> row_pointers;
    get_row_pointers(base, row_pointers);
    int width, height, channels;
    std::string mini_map = "maps/no-border/" + map_name + "_" + game_mode + ".png";
    read_png(mini_map, &row_pointers[0], width, height, channels);
};

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
        }
        return new process_result{ file_path, 500, 500 };
    };

    auto f_create_replays = [](process_result* result) -> process_result* {
        if (result == nullptr) {
            result->error = true;
            return result;
        }
        try {
            // std::cout << result->path << "\n";
            ifstream is(result->path, std::ios::binary);
            result->replay = new parser(is);
            is.close();
            // std::cout << result->replay->get_version() << "\n";
        } catch (std::exception &e) {
            result->error = true;
            std::cerr << "Error!" << std::endl;
            result->replay = nullptr;
        }
        return result;
    };

    auto f_draw_position = [](const packet_t &packet, process_result &result, std::array<image_t<float>, 2> &images) {
        uint32_t player_id = packet.player_id();
        const parser &replay = *result.replay;
        const game_info &game_info = replay.get_game_info();

        int team_id = get_team(game_info, player_id);
        if (team_id < 0) return;

        auto shape = result.position_image[0].shape();
        int width = static_cast<int>(shape[1]);
        int height = static_cast<int>(shape[0]);
        
        float x,y;
        auto position = packet.position();
        std::tie(x,y) = get_2d_coord(position, game_info, width, height);
        
        if (__builtin_expect(x >= 0 && y >= 0 && x <= (width - 1) && y <= (height - 1), 1)) {
            float px = x - floor(x), py = y - floor(y);
            images[team_id][floor(y)][floor(x)] += px*py;
            images[team_id][ceil(y)][floor(x)] += px*(1-py);
            images[team_id][floor(y)][ceil(x)] += (1-px)*py;
            images[team_id][ceil(y)][ceil(x)] += (1-px)*(1-py);
        }
    };
    
    auto f_draw_death = [](const packet_t &packet, process_result &result) {
        
    };
    
    auto f_process_replays = [&](process_result *result) -> process_result* {
        if (result->error) return result;

        std::fill(result->position_image[0].data(), result->position_image[0].data() + 500*500, 0.f);
        const parser &replay = *result->replay;
        std::set<uint32_t> dead_players;

        for (const packet_t &packet : replay.get_packets()) {
            if (packet.has_property(property::tank_destroyed)) {
                f_draw_death(packet, *result);
                uint32_t killer, killed;
                std::tie(killed, killer) = packet.tank_destroyed();
                dead_players.insert(killed);
                packet_t position_packet;
                bool found = result->replay->find_property(packet.clock(), killed, property::position, position_packet);
                if (found) {
                    f_draw_position(position_packet, *result, result->death_image);
                }
            }
            if (packet.has_property(property::position)
                && packet.has_property(property::player_id)
                && dead_players.find(packet.player_id()) == dead_players.end()) {
                f_draw_position(packet, *result, result->position_image);
            }
            
        }
        return result;
    };

    auto f_create_image = [](process_result* result) -> process_result* {
        if (result->error) return result;
        boost::multi_array<uint8_t, 3> base(boost::extents[500][500][4]);
        std::unique_ptr<png_bytep[]> row_pointers(new png_bytep[500]);
        auto shape = base.shape();
        for (int i = 0; i < shape[0]; ++i) {
            row_pointers[i] = base.origin() + i*shape[1]*shape[2];
        }
        int width, height, channels;
        const parser &replay = *result->replay;
        read_png(replay.get_game_info().mini_map, row_pointers.get(), width, height, channels);
        bool alpha = channels == 4;
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
        std::stringstream out_file_name;
        out_file_name << "out/positions/" << result->path << ".png";
        write_png(out_file_name.str(), row_pointers.get(), width, height, alpha);
        return result;
    };


    auto merge_image = [] (image_t<float> &left, image_t<float> &right) {
        return [&]( const blocked_range<size_t>& range )  {
            for (size_t i = range.begin(); i != range.end(); ++i) {
                left.data()[i] += right.data()[i];
            }
        };
    };
    
    std::map<std::tuple<std::string, std::string>, std::array<image_t<float>, 4>> images;
    auto f_merge_results = [&](process_result *result) -> process_result* {
        if (result->error || result->replay->get_game_end().size() == 0 ) return result;

        const parser &replay = *result->replay;
        const game_info &game_info = replay.get_game_info();

        auto key = std::make_tuple(game_info.map_name, game_info.game_mode);
        if (images.find(key) == images.end()) {
            images.insert(std::make_pair(key,
                std::array<image_t<float>,4>({
                    image_t<float>(boost::extents[500][500]),
                    image_t<float>(boost::extents[500][500]),
                    image_t<float>(boost::extents[500][500]),
                    image_t<float>(boost::extents[500][500])
                })));
        }

        std::array<image_t<float>,4> &image = images[key];

        auto shape = image[0].shape();
        auto size = shape[0]*shape[1];
        
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


    auto get_bounds = [](const image_t<float> &image, float l_quant,float r_quant) -> std::tuple<float, float> {
        accumulator_t_right r(tag::tail<boost::accumulators::right>::cache_size = c);
        accumulator_t_left l(tag::tail<boost::accumulators::left>::cache_size = c);
        auto shape = image.shape();
        int size = static_cast<int>( shape[0]*shape[1] );
        for (int i = 0; i < size; ++i) {
            if (*(image.data() + i) > 0.f) {
            r(*(image.data() + i));
            l(*(image.data() + i));
            }
        }
        auto lower_bound = quantile(l, quantile_probability = l_quant );
        auto upper_bound = quantile(r, quantile_probability = r_quant );
        return std::make_tuple(lower_bound, upper_bound);
    };

    auto calculate_alpha = [](const std::tuple<float, float> &bounds, float value) -> float {
        return
        std::min(std::max((value - std::get<0>(bounds)) / (std::get<1>(bounds) - std::get<0>(bounds)), 0.f), 1.f);
    };

    auto mix = [](int v0, int v1, float a1, int v2, float a2) {
        return (1-a2)*((1-a1)*v0 + a1*v1) + a2 * v2;
    };
    
    auto draw_image = [&](boost::multi_array<uint8_t, 3> &base, const image_t<float> &team1, const image_t<float> &team2, float l_quant, float u_quant) {
        std::array<std::tuple<float, float>, 2> bounds = {
            get_bounds(team1, l_quant, u_quant),
            get_bounds(team2, l_quant, u_quant)
        };

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

    typedef std::map<std::tuple<std::string, std::string>, std::array<image_t<float>, 4>>::value_type images_entry;
    tbb::parallel_do(images.begin(), images.end(), [&](const images_entry &entry){
        const std::array<image_t<float>,4> &image = entry.second;
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


auto get_bounds(const std::vector<float> &values) -> std::tuple<float, float> {
    accumulator_t_right r(tag::tail<boost::accumulators::right>::cache_size = c);
    accumulator_t_left l(tag::tail<boost::accumulators::left>::cache_size = c);
    for (auto value : values) {
        r(value);
        l(value);
    }
    auto lower_bound = quantile(l, quantile_probability = 0.05 );
    auto upper_bound = quantile(r, quantile_probability = 0.95 );
    return std::make_tuple(lower_bound, upper_bound);
};

int main(int argc, const char * argv[]) {
    chdir("/Users/jantemmerman/Development/wotreplay-parser/data");
    
    // process_replay_directory("replays"); std::exit(1);
    
    string file_names[] = {
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

    auto file_name = file_names[2];
    ifstream is(file_name, std::ios::binary);

    if (!is) {
        std::cerr << "Something went wrong with reading file: " << file_name << std::endl;
        std::exit(-1);
    }
    
    parser replay(is);
    is.close();

    // display some info about the replay
    const auto &game_info = replay.get_game_info();
    show_packet_summary(replay.get_packets());
    write_parts_to_file(replay);
    show_map_boundaries(game_info, replay.get_packets());
    write_parts_to_file(replay);

    // create image
    boost::multi_array<uint8_t, 3> image;
    read_mini_map(game_info.map_name, game_info.game_mode, image);
    create_image(image, replay);
    write_image("out/replay.png", image);
    return EXIT_SUCCESS;
}