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
#include <vector>
#include <array>
#include "json/json.h"
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>



using std::ifstream;
using std::ios;
using std::ofstream;
using std::ostream_iterator;
using std::string;
using std::vector;

using wotstats::replay_file;
using wotstats::buffer_t;
using wotstats::slice_t;
using wotstats::packet_t;

struct map_info {
    std::array<std::tuple<int,int>, 2> limits;
    std::string mini_map;
};

struct game_info {
    std::string map_name;
    std::array<std::vector<int>, 2> teams;
    unsigned recorder_id;
};

double round(double number)
{
    return number < 0.0 ? ceil(number - 0.5) : floor(number + 0.5);
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

std::tuple<int, int>
get_2d_coord(const std::tuple<float, float, float> &position, const map_info &map_info, int width, int height) {
    int x,y,z, min_x, max_x, min_y, max_y;
    std::tie(x,z,y) = position;
    std::tie(min_x, max_x) = map_info.limits[0];
    std::tie(min_y, max_y) = map_info.limits[1];
    x = round((x - min_x) / static_cast<float>(max_x - min_x) * width);
    y = round(((max_y - min_y) - (y - min_y)) / static_cast<float>(max_y - min_y) * height);
    return std::make_tuple(x,y);
}

void display_packet_clock(const packet_t &packet) {
    if (packet.data.size() < 9) return;
    std::cout << *reinterpret_cast<const int*>(&packet.data[5]) << "\n";
}

void create_image(
                  unsigned char **image,
                  int width, int height,
                  const std::vector<packet_t> &packets,
                  const game_info &game_info,
                  const map_info &map_info,
                  bool alpha
                  ) {
    for (auto it = packets.begin(); it != packets.end(); ++it) {
        const packet_t &packet = *it;
        switch(packet.type()) {
            case 0x0a: {
                int player_id = (*(const unsigned*) &packet.data[9]);
                auto position = packet.position();
                int x,y;
                auto teams = game_info.teams;
                std::tie(x,y) = get_2d_coord(position, map_info, width, height);
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
            case 0x07: {
                char sub_type = packet.data[13];
                if (sub_type != 0x02)
                    continue;
                short health = packet.health();
                if (health == 0) {
                    auto result = std::find_if(packets.rbegin(), packets.rend(), [&it](const packet_t &packet) -> bool {
                        return packet.type() == 0x0a
                            && packet.has_property(wotstats::property::player_id)
                            && (*it).has_property(wotstats::property::player_id)
                            && packet.player_id() == (*it).player_id()
                            && packet.clock() == (*it).clock();
                    });
                    if (result != packets.rend()) {
                        std::tuple<float, float, float> position = (*result).position();
                        
                        int offsets[][2] = {
                            {-1, -1},
                            {-1, 0},
                            {-1, 1},
                            {0, 1},
                            {1, 1},
                            {1, 0},
                            {1, -1},
                            {0 , -1},
                            {0, 0}
                        };
                        for (auto offset : offsets) {
                            int x, y;
                            std::tie(x,y) = get_2d_coord(position, map_info, width, height);
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

    }
}

void write_parts_to_file(const replay_file &replay) {
    ofstream game_begin("/Users/jantemmerman/Development/wotreplays/game_begin.txt", ios::binary | ios::ate);
    std::copy(replay.get_game_begin().begin(),
              replay.get_game_begin().end(),
              ostream_iterator<char>(game_begin));
    game_begin.close();
    
    ofstream game_end("/Users/jantemmerman/Development/wotreplays/game_end.txt", ios::binary | ios::ate);
    std::copy(replay.get_game_end().begin(),
              replay.get_game_end().end(),
              ostream_iterator<char>(game_end));
    game_end.close();
    
    ofstream replay_content("/Users/jantemmerman/Development/wotreplays/replay.dat", ios::binary | ios::ate);
    std::copy(replay.get_replay().begin(),
              replay.get_replay().end(),
              ostream_iterator<char>(replay_content));
}

game_info get_game_info(const replay_file& replay) {
    std::array<std::vector<int>, 2> teams;
    Json::Value root;
    Json::Reader reader;
    const buffer_t &game_begin = replay.get_game_begin();
    std::string doc(game_begin.begin(), game_begin.end());
    reader.parse(doc, root);
    Json::Value vehicles = root["vehicles"];
    std::string player_name = root["playerName"].asString();
    unsigned recorder_id = -1;
    for (auto it = vehicles.begin(); it != vehicles.end(); ++it) {
        unsigned player_id = boost::lexical_cast<int>(it.key().asString());
        std::string name = (*it)["name"].asString();
        if (name == player_name) {
            recorder_id = player_id;
        }
        int team_id = (*it)["team"].asInt();
        teams[team_id - 1].push_back(player_id);
    }

    std::string map_name = root["mapName"].asString();
    
    return { map_name, teams, recorder_id };
}



void display_boundaries(const game_info &game_info, std::vector<packet_t> packets) {

    float min_x = 0.f, min_y = 0.f, max_x = 0.f, max_y = 0.f;

    for (auto packet : packets) {
        if (packet.type() != 0xa) {
            continue;
        }
        
        int player_id = (*(const unsigned*) &packet.data[9]);
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

void print_packet(const slice_t &packet) {
    char packet_type = packet[1];
    // if (packet_type != 0x08) return;
    if (!(packet_type == 0x07)) return;
    // auto player_id = *(const unsigned int*) &packet[9];
    // auto val = *(const short int*) &packet[21];
    // if (player_id != 139174664) return;
    // int i = 0;
    // std::cout << val << "\n";
    for (auto val : packet) {
        unsigned ival = (unsigned)(unsigned char)(val);
        printf("%02X ", ival);
       // ++i;
    }
    printf("\n");
    // printf("%d\n", i);
}

using namespace boost::filesystem;

void process_reference_data() {
    path directory("/Users/jantemmerman/Development/wotreplays/data/");
    std::vector<directory_entry> file_names;
    std::copy(directory_iterator(directory),
              directory_iterator(),
              std::insert_iterator<std::vector<directory_entry>>(file_names, file_names.begin()));
    for (auto entry : file_names) {
        std::cout << entry.path().string() << "\n";
        ifstream is(entry.path().string(), std::ios::binary);

        if (!is) {
            std::cerr << "Something went wrong with reading file: " << entry << std::endl;
            std::exit(-1);
        }

        try {
        replay_file replay(is);
        auto game_info = get_game_info(replay);
        is.close();

        std::vector<packet_t> packets;
        replay.get_packets(packets);
        } catch (const std::exception &e) {
            std::cout << "Error processing file: " << entry << "\n";
        }
    }
}

int main(int argc, const char * argv[])
{
    // std::exit(1);
    // process_reference_data();
    
    // std::exit(1);
    // string replay_file_name("/Users/jantemmerman/Development/wotreplays/20120707_2059_germany-E-75_himmelsdorf.wotreplay");
    // string replay_file_name("/Users/jantemmerman/Development/wotreplays/siegfried_line/20120628_2039_germany-E-75_siegfried_line.wotreplay");
    string replay_file_name = "/Users/jantemmerman/Development/wotreplays/20120815_0309_germany-E-75_02_malinovka.wotreplay";
    // string replay_file_name = "/Users/jantemmerman/Development/wotreplays/20120826_0013_france-AMX_13_90_04_himmelsdorf.wotreplay";
    // string replay_file_name = "/Users/jantemmerman/Development/wotreplays/20120826_0019_france-AMX_13_90_45_north_america.wotreplay";
    // replay_file_name = "/Users/jantemmerman/Development/wotreplays/data/20120701_1247_germany-E-75_monastery.wotreplay";
    // string replay_file_name = "/Users/jantemmerman/Development/wotreplays/20120826_2059_france-AMX_13_90_02_malinovka.wotreplay";
    // string replay_file_name = "/Users/jantemmerman/Development/wotreplays/20120826_1729_france-AMX_13_90_04_himmelsdorf.wotreplay";
    ifstream is(replay_file_name, std::ios::binary);

    if (!is) {
        std::cerr << "Something went wrong with reading file: " << replay_file_name << std::endl;
        std::exit(-1);
    }
    
    replay_file replay(is);
    auto game_info = get_game_info(replay);
    is.close();
    
    std::vector<packet_t> packets;
    replay.get_packets(packets);

    // display_packet_summary(packets);
    // display_boundaries(game_info, packets);
    
    write_parts_to_file(replay);
    std::map<std::string, map_info> map_info = {
        {"04_himmelsdorf", {
            .limits = {
                std::make_tuple(-300, 400),
                std::make_tuple(-300, 400)
            },
            .mini_map = "/Users/jantemmerman/Development/wotreplays/himmelsdorf-5.png"
        }},
        {"45_north_america", {
            .limits = {
                std::make_tuple(-500, 500),
                std::make_tuple(-500, 500)
            },
            .mini_map = "/Users/jantemmerman/Development/wotreplays/45_north_america_ctf.png"
        }},
        {"02_malinovka", {
            .mini_map = "/Users/jantemmerman/Development/wotreplays/02_malinovka_ctf.png",
            .limits = {
                std::make_tuple(-500, 500),
                std::make_tuple(-500, 500)
            }
        }}
    };

    if (map_info.find(game_info.map_name) != map_info.end()) {
        png_bytepp row_pointers;
        int width, height, channels;
        read_png(map_info[game_info.map_name].mini_map, row_pointers, width, height, channels);
        bool alpha = channels == 4;
        create_image(row_pointers, width, height, packets, game_info, map_info[game_info.map_name], alpha);
        write_png("/Users/jantemmerman/Development/wotreplays/replay.png", row_pointers, width, height, alpha);
    }

    return EXIT_SUCCESS;
}