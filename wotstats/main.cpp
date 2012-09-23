//
//  main.cpp
//  wotstats
//
//  Created by Jan Temmerman on 14/05/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "replay_file.h"
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

using namespace std;
using namespace wotstats;
using namespace tbb;
using namespace boost::filesystem;

template <typename T>
T round(T number) {
    return number < static_cast<T>(0.0) ? ceil(number - static_cast<T>(0.5)) : floor(number + static_cast<T>(0.5));
}


void display_packet_summary(const std::vector<packet_t>& packets) {
    std::map<char, int> packet_type_count;

    for (auto p : packets) {
        packet_type_count[p.type()]++;
    }
    
    for (auto it : packet_type_count) {
        printf("packet_type [%02x] = %d\n", it.first, it.second);
    }
    printf("Total packets = %lu\n", packets.size());

}

std::tuple<int, int> get_2d_coord(const std::tuple<float, float, float> &position, const game_info &game_info, int width, int height) {
    int x,y,z, min_x, max_x, min_y, max_y;
    std::tie(x,z,y) = position;
    std::tie(min_x, max_x) = game_info.boundaries[0];
    std::tie(min_y, max_y) = game_info.boundaries[1];
    x = round((x - min_x) * (width / static_cast<float>(max_x - min_x + 1)));
    y = round((max_y - y) * (height / static_cast<float>(max_y - min_y + 1)));
    return std::make_tuple(x,y);
}

void display_packet_clock(const packet_t &packet) {
    std::cout << packet.clock() << "\n";
}

void create_image(
                  unsigned char **image,
                  int width, int height,
                  const replay_file &replay,
                  const game_info &game_info,
                  bool alpha
                  ) {
    const std::vector<packet_t> &packets = replay.get_packets();
    for (auto it = packets.begin(); it != packets.end(); ++it) {
        const packet_t &packet = *it;
        if (packet.has_property(property::position)) {
                int player_id = (*(const unsigned*) &packet.get_data()[9]);
                auto position = packet.position();
                int x,y;
                auto teams = game_info.teams;
                std::tie(x,y) = get_2d_coord(position, game_info, width, height);
                x *= alpha ? 4 : 3;

                if (alpha) image[y][x+3] = 0xff;

                if (packet.player_id() == game_info.recorder_id) {
                    image[y][x] = image[y][x+1] = 0x00;
                    image[y][x + 2] = 0xff;
                    continue;
                }

                int team_id = -1;
                for (auto it = teams.begin(); it != teams.end(); ++it) {
                    auto pos = std::find(it->begin(), it->end(), player_id);
                    if (pos != it->end()) {
                        team_id = static_cast<int>(it - teams.begin());
                        break;
                    }
                }

                if (team_id == -1) {
                    continue;
                }
        
                switch (team_id) {
                    case 0:
                        image[y][x] = image[y][x+2] = 0x00;
                        image[y][x + 1] = 0xff;
                        break;
                    case 1:
                        image[y][x]= 0xff;
                        image[y][x+1] = image[y][x+2] = 0x00;
                        break;
                }              
        }

        if (packet.has_property(property::tank_destroyed)) {
            uint32_t target, killer;
            std::tie(target, killer) = packet.tank_destroyed();
            packet_t position_packet;

            bool found = replay.find_property(packet.clock(), target, property::position, position_packet);
            if (found) {
                std::tuple<float, float, float> position = position_packet.position();
                int offsets[][2] = {
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
                    int x, y;
                    std::tie(x,y) = get_2d_coord(position, game_info, width, height);
                    x += offset[0];
                    y += offset[1];
                    x *= alpha ? 4 : 3;
                    if (alpha) image[y][x+3] = 0xff;
                    image[y][x] = image[y][x+1] = 0xff;
                    image[y][x + 2] = 0x00;
                }
            }
        }
    }
}

void write_parts_to_file(const replay_file &replay) {
    ofstream game_begin("out/game_begin.txt", ios::binary | ios::ate);
    std::copy(replay.get_game_begin().begin(),
              replay.get_game_begin().end(),
              ostream_iterator<char>(game_begin));
    game_begin.close();
    
    ofstream game_end("out/game_end.txt", ios::binary | ios::ate);
    std::copy(replay.get_game_end().begin(),
              replay.get_game_end().end(),
              ostream_iterator<char>(game_end));
    game_end.close();
    
    ofstream replay_content("out/replay.dat", ios::binary | ios::ate);
    std::copy(replay.get_replay().begin(),
              replay.get_replay().end(),
              ostream_iterator<char>(replay_content));
}

void display_boundaries(const game_info &game_info, std::vector<packet_t> packets) {

    float min_x = 0.f, min_y = 0.f, max_x = 0.f, max_y = 0.f;

    for (auto packet : packets) {
        if (packet.type() != 0xa) {
            continue;
        }
        
        uint32_t player_id = packet.player_id();
        auto position = packet.position();

        int team_id = -1;
        auto teams = game_info.teams;
        for (auto it = teams.begin(); it != teams.end(); ++it) {
            auto pos = std::find(it->begin(), it->end(), player_id);
            if (pos != it->end()) {
                team_id = static_cast<int>(it - teams.begin());
                break;
            }
        }
   
        if (team_id == -1) {
            continue;
        }

        min_x = std::min(min_x, std::get<0>(position));
        min_y = std::min(min_y, std::get<2>(position));
        max_x = std::max(max_x, std::get<0>(position));
        max_y = std::max(max_y, std::get<2>(position));
    }
    
    printf("%f %f %f %f\n", min_x, max_x, min_y, max_y);
}

bool write_png(const std::string &out, png_bytepp image, int width, int height, bool alpha) {
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
    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_rows(png_ptr, info_ptr, image);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    
    return true;
}

void read_png(const std::string &in, png_bytepp &image, int &width, int &height, int &channels) {
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
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);
    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    width = png_get_image_width(png_ptr, info_ptr);
    channels = png_get_channels(png_ptr, info_ptr);
    image = new png_bytep[height];
    for (int i = 0; i < height; ++i) {
        image[i] = new png_byte[width*channels];
        std::copy_n(row_pointers[i], width*channels, image[i]);
    }
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(fp);
    return;
}

void read_png2(const std::string &in, png_bytepp image, int &width, int &height, int &channels) {
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
    fclose(fp);
}

void print_packet(const slice_t &packet) {
    for (auto val : packet) {
        unsigned ival = (unsigned)(unsigned char)(val);
        printf("%02X ", ival);
    }
    printf("\n");
}

template <typename T>
using image_t = boost::multi_array<T, 2>;

struct process_result {
    bool error;
    std::string path;
    replay_file *replay;
    array<image_t<int>, 2> position_image, death_image, hit_image;
    process_result(std::string path, int image_width, int image_height)
        : path(path), position_image({
            image_t<int>(boost::extents[image_width][image_height]),
            image_t<int>(boost::extents[image_width][image_height])
        }), death_image({
            image_t<int>(boost::extents[image_width][image_height]),
            image_t<int>(boost::extents[image_width][image_height])
        }), hit_image({
            image_t<int>(boost::extents[image_width][image_height]),
            image_t<int>(boost::extents[image_width][image_height])
        }), replay(nullptr), error(false)
    {}
    process_result(const process_result&) = delete;
    process_result & operator= (const process_result & other) = delete;
    ~process_result() {
        delete replay;
    }
};

int get_team(const game_info &game_info, uint32_t player_id) {
    const auto &teams = game_info.teams;
    auto it = std::find_if(teams.begin(), teams.end(), [&](const std::set<int> &team) {
        return team.find(player_id) != team.end();
    });
    return it == teams.end() ? -1 : (static_cast<int>(it - teams.begin()));
}

void process_replay_directory(const path& directory) {
    std::map<std::string, std::vector<png_bytepp>> images;
    
    directory_iterator it(directory);
    // get out if no elements
    if (it == directory_iterator()) return;
    
    auto f_generate_paths = [&it](tbb::flow_control &fc) -> process_result* {
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
            ifstream is(result->path, std::ios::binary);
            result->replay = new replay_file(is);
            is.close();
        } catch (exception &e) {
            result->error = true;
            std::cerr << "Error!" << std::endl;
            result->replay = nullptr;
        }
        return result;
    };

    auto f_draw_position = [](const packet_t &packet, process_result &result) {
        uint32_t player_id = packet.player_id();
        const replay_file &replay = *result.replay;
        const game_info &game_info = replay.get_game_info();

        int team_id = get_team(game_info, player_id);
        if (team_id < 0) return;

        auto shape = result.position_image[0].shape();
        int width = static_cast<int>(shape[1]), height = static_cast<int>(shape[0]);
        
        int x,y;
        auto position = packet.position();
        std::tie(x,y) = get_2d_coord(position, game_info, width, height);
        
        if (__builtin_expect(x < 0 || y < 0 || x >= width || y >= height, 0)) {
            if (!result.error) {
#if DEBUG
            std::cerr
                << "WARNING: incorrect limits for " << game_info.map_name
                << " x: " << x << " y: " << y
                << " (" << std::get<0>(position) << "/" << std::get<2>(position) << ")"
                << std::endl;
#endif
            result.error = true;
            }
        } else {
            result.position_image[team_id][y][x]++;
        }
    };
    
    auto f_draw_death = [](const packet_t &packet, process_result &result) {

    };
    
    auto f_process_replays = [&](process_result *result) -> process_result* {
        if (result->error) return result;
        const replay_file &replay = *result->replay;
        for (const packet_t &packet : replay.get_packets()) {
            if (packet.has_property(property::position)
                && packet.has_property(property::player_id)) {
                f_draw_position(packet, *result);
            }
            if (packet.has_property(property::tank_destroyed)) {
                f_draw_death(packet, *result);
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
        const replay_file &replay = *result->replay;
        read_png2(replay.get_game_info().mini_map, row_pointers.get(), width, height, channels);
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
        out_file_name << "out/" << result->path << ".png";
        write_png(out_file_name.str(), row_pointers.get(), width, height, alpha);
        return result;
    };

    auto f_merge_results = [](process_result *result) -> process_result* {
        return result;
    };

    auto f_clean_up = [](process_result* result) -> void {
        delete result;
    };

    tbb::parallel_pipeline(10,
                           tbb::make_filter<void, process_result*>(tbb::filter::serial_in_order, f_generate_paths) &
                           tbb::make_filter<process_result*, process_result*>(tbb::filter::parallel, f_create_replays) &
                           tbb::make_filter<process_result*, process_result*>(tbb::filter::parallel, f_process_replays) &
                           tbb::make_filter<process_result*, process_result*>(tbb::filter::parallel, f_create_image) &
                           tbb::make_filter<process_result*, process_result*>(tbb::filter::serial_out_of_order, f_merge_results) &
                           tbb::make_filter<process_result*, void>(tbb::filter::parallel, f_clean_up));
}

int main(int argc, const char * argv[]) {
    chdir("/Users/jantemmerman/Development/wotstats/data");

     // process_replay_directory("replays"); std::exit(1);
    
    string file_names[] = {
        "replays/20120707_2059_germany-E-75_himmelsdorf.wotreplay",
        "replays/20120815_0309_germany-E-75_02_malinovka.wotreplay",
        "replays/8.0/20120906_2352_germany-Panther_II_02_malinovka.wotreplay",
        "replays/20120826_0013_france-AMX_13_90_04_himmelsdorf.wotreplay",
        "replays/20120826_0019_france-AMX_13_90_45_north_america.wotreplay",
        "replays/20120701_1247_germany-E-75_monastery.wotreplay",
        "replays/20120826_2059_france-AMX_13_90_02_malinovka.wotreplay",
        "replays/20120826_1729_france-AMX_13_90_04_himmelsdorf.wotreplay",
        "replays/20120920_2130_france-AMX_13_90_14_siegfried_line.wotreplay",
        "replays/20120921_0042_ussr-IS-3_02_malinovka.wotreplay"
    };

    auto file_name = file_names[8];
    ifstream is(file_name, std::ios::binary);

    if (!is) {
        std::cerr << "Something went wrong with reading file: " << file_name << std::endl;
        std::exit(-1);
    }
    
    replay_file replay(is);
    const auto &game_info = replay.get_game_info();
    is.close();

    display_packet_summary(replay.get_packets());
    display_boundaries(game_info, replay.get_packets());
    
    write_parts_to_file(replay);

    const std::vector<packet_t> &packets = replay.get_packets();
    auto has_clock = [](const packet_t &packet) -> bool { return packet.has_property(property::clock) && packet.clock() != 0; };
    auto first_clock_packet = std::find_if(packets.cbegin(), packets.cend(), has_clock);
    auto last_clock_packet = std::find_if(packets.crbegin(), packets.crend(), has_clock);
    
    std::cout << first_clock_packet->clock() << " " << last_clock_packet->clock() << " "
        << last_clock_packet->clock() - first_clock_packet->clock() << "\n";
    float prev = 0 , clock = 0;
    for (const packet_t &packet : packets) {
        auto data = packet.get_data();
        if (packet.has_property(property::clock) && prev
                != (clock = get_field<float>(data.begin(), data.end(), 5))) {
            
            std::cout <<clock << " (diff: " <<  clock - prev << ")\n";
            prev = clock;
        } 

        //print_packet(data);
    }
    png_bytepp row_pointers;
    int width, height, channels;
    read_png(game_info.mini_map, row_pointers, width, height, channels);
    bool alpha = channels == 4;
    create_image(row_pointers, width, height, replay, game_info, alpha);
    write_png("out/replay.png", row_pointers, width, height, alpha);
       return EXIT_SUCCESS;
}