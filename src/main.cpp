#include "image_util.h"
#include "image_writer.h"
#include "json_writer.h"
#include "json/json.h"
#include "parser.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/multi_array.hpp>
#include <boost/program_options.hpp>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <tbb/tbb.h>
#include <tuple>
#include <vector>

using namespace std;
using namespace wotreplay;
using namespace tbb;
using namespace boost::filesystem;
using namespace boost;

namespace po = boost::program_options;


#define MAP_SIZE    512

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
    game_t replay;
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
        : error(false), path(path)
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
    bool found = result.replay.find_property(packet.clock(), killed, property_t::position, position_packet);
    if (found) {
        draw_position(position_packet, result.replay, result.death_image);
    }
}

void process_replay_directory2(const path& directory) {
    directory_iterator it(directory);

    // get out if no elements
    if (it == directory_iterator()) return;

    
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
            const auto &path = it->path();
            file_path = path.string();
            ++count;
        }
        return new process_result{ file_path, MAP_SIZE, MAP_SIZE };
    };

    auto f_create_replays = [](process_result* result) -> process_result* {
        if (result == nullptr) {
            result->error = true;
            return result;
        }
        try {
            ifstream is(result->path, std::ios::binary);
            parser_t parser;
            parser.parse(is, result->replay);
            is.close();
        } catch (std::exception &e) {
            result->error = true;
            std::cerr << "Error!" << std::endl;
        }
        return result;
    };

    auto f_process_replays = [](process_result *result) -> process_result* {
        if (result->error) return result;

        const game_t &replay = result->replay;
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
        boost::multi_array<uint8_t, 3> base(boost::extents[MAP_SIZE][MAP_SIZE][4]);

        const game_t &replay = result->replay;
        
        read_mini_map(replay.get_map_name(), replay.get_game_mode(), base);

        for (int i = 0; i < MAP_SIZE; ++i) {
            for (int j = 0; j < MAP_SIZE; ++j) {
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
        std::ofstream file(death_image.str(), std::ios::binary);
        write_png(file, base);
        return result;
    };
    
    std::map<std::tuple<std::string, std::string>, boost::multi_array<float,3>> images;
    auto f_merge_results = [&](process_result *result) -> process_result* {
        if (result->error /* || result->replay->get_game_end().size() == 0 */ ) return result;

        const game_t &replay = result->replay;

        auto key = std::make_tuple(replay.get_map_name(), replay.get_game_mode());
        if (images.find(key) == images.end()) {
            images.insert({key, boost::multi_array<float,3>(boost::extents[4][MAP_SIZE][MAP_SIZE])});
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
        std::string position_image = (boost::format("out/positions/%1%/%2%.png") % map_name % game_mode).str();
        std::ofstream position_image_file(position_image, std::ios::binary);
        write_png(position_image_file, base);

        // process deaths
        read_mini_map(map_name, game_mode, base);
        draw_image(base, image[2], image[3], .33, .66);
        std::string death_image = (boost::format("out/deaths/%1%/%2%.png") % map_name % game_mode).str();
        std::ofstream death_image_file(death_image, std::ios::binary);
        write_png(death_image_file, base);
    });
}

void dump_positions(const game_t &game) {
    std::ofstream os("positions.csv");
    os << "clock;player_id;team_id;x;z;y\n";
    const std::vector<packet_t> &packets = game.get_packets();
    for (const packet_t &packet : packets) {
        if (packet.has_property(property_t::position)) {
            auto position = packet.position();
            os << packet.clock() << ";" << packet.player_id() << ";" << game.get_team_id(packet.player_id()) << ";"
                << std::get<0>(position) << ";" << std::get<1>(position) << ";" << std::get<2>(position) << std::endl;
        }
    }
    os.close();
}

void show_help(int argc, const char *argv[], po::options_description &desc) {
    std::string program_name(argv[0]);
    std::cout
        << boost::format("Usage: %1% --root <working directory> --type <output type> --input <input file> --output <output file>\n\n") % program_name
        << desc << "\n";
}

bool has_required_options(po::variables_map &vm) {
    return vm.count("output") > 0 || vm.count("type") > 0
            || vm.count("root") > 0|| vm.count("input") > 0;
}

int main(int argc, const char * argv[]) {
    po::options_description desc("Allowed options");

    std::string type, output, input, root;
    
    desc.add_options()
        ("type,t"  , po::value(&type), "select output type")
        ("output,o", po::value(&output), "target file")
        ("input,i" , po::value(&input), "input file")
        ("root,r"  , po::value(&root), "set root directory")
        ("help,h"  , "produce help message")
        ("debug"   , "enable parser debugging");

    po::variables_map vm;
    
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (std::exception &e) {
        show_help(argc, argv, desc);
        std::exit(-1);
    } catch (...) {
        std::cerr << "Unknown error." << std::endl;
        std::exit(-1);
    }

    if (vm.count("help")
            || !has_required_options(vm)) {
        show_help(argc, argv, desc);
        std::exit(0);
    }

    if (chdir(root.c_str()) != 0) {
        std::cerr << boost::format("cannot change working directory to: %1%\n") % root;
        std::exit(0);
    }

    ifstream is(input, std::ios::binary);

    if (!is) {
        std::cerr << boost::format("Something went wrong with reading file: %1%\n") % input;
        std::exit(0);
    }

    parser_t parser;
    game_t game;
    
    bool debug = vm.count("debug") > 0;
    parser.set_debug(debug);
    parser.parse(is, game);
    is.close();

    std::unique_ptr<writer_t> writer;
    
    if (type == "png") {
        writer = std::unique_ptr<writer_t>(new image_writer_t());
        auto &image_writer = dynamic_cast<image_writer_t&>(*writer);
        image_writer.set_show_self(true);
    } else if (type == "json") {
        writer = std::unique_ptr<writer_t>(new json_writer_t());
    } else {
        std::cout << "Invalid output type, supported types: png" << std::endl;
    }

    writer->init(game.get_map_name(), game.get_game_mode());
    writer->update(game);
    writer->finish();
    
    std::ofstream file(output);
    writer->write(file);
    
    return EXIT_SUCCESS;
}
