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
#include <tbb/tbb.h>

using namespace std;
using namespace wotstats;
using namespace tbb;

struct map_info {
    std::array<std::tuple<int,int>, 2> limits;
    std::string mini_map;
};

struct game_info {
    std::string map_name;
    std::string game_mode;
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
    std::cout << packet.clock() << "\n";
}

void create_image(
                  unsigned char **image,
                  int width, int height,
                  const replay_file &replay,
                  const game_info &game_info,
                  const map_info &map_info,
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

        if (packet.has_property(property::health)) {
           uint16_t health = packet.health();
           if (health == 0) {
               packet_t position_packet;
               auto packet_id = std::distance(packets.begin(), it);
               bool found = replay.find_property(packet_id, property::position, position_packet);
               if (found) {
                   std::tuple<float, float, float> position = position_packet.position();                   
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

                   // std::cout << packet.player_id() << "\n";
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
    std::string game_mode = root["gameplayType"].asString();
    return { map_name, game_mode, teams, recorder_id };
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

void print_packet(const slice_t &packet) {
    for (auto val : packet) {
        unsigned ival = (unsigned)(unsigned char)(val);
        printf("%02X ", ival);
    }
    printf("\n");
}

using namespace boost::filesystem;

void process_reference_data() {
    path directory("data/");
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
        is.close();
        
        } catch (const std::exception &e) {
            std::cout << "Error processing file: " << entry << "\n";
        }
    }
}

bool get_map_info(const game_info &game_info, map_info &map_info) {
    std::map<std::string, std::array<std::tuple<int,int>, 2>> map_boundaries = {
        { "01_karelia",         { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "02_malinovka",       { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "03_campania",        { std::make_tuple(-300, 300), std::make_tuple(-300, 300) } },
        { "04_himmelsdorf",     { std::make_tuple(-300, 400), std::make_tuple(-300, 400) } },
        { "05_prohorovka", 	    { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "06_ensk",            { std::make_tuple(-300, 300), std::make_tuple(-300, 300) } },
        { "07_lakeville",       { std::make_tuple(-400, 400), std::make_tuple(-400, 400) } },
        { "08_ruinberg",        { std::make_tuple(-400, 400), std::make_tuple(-400, 400) } },
        { "10_hills",           { std::make_tuple(-400, 400), std::make_tuple(-400, 400) } },
        { "11_murovanka",       { std::make_tuple(-400, 400), std::make_tuple(-400, 400) } },
        { "13_erlenberg",       { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "14_siegfried_line",  { std::make_tuple(-400, 400), std::make_tuple(-400, 400) } },
        { "15_komarin",         { std::make_tuple(-400, 400), std::make_tuple(-400, 400) } },
        { "17_munchen",         { std::make_tuple(-300, 300), std::make_tuple(-300, 300) } },
        { "18_cliff",           { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "19_monastery",       { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "22_slough",          { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "23_westfeld",        { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "28_desert",          { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "29_el_hallouf",      { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "31_airfield",        { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "33_fjord",           { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "34_redshire",        { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "35_steppes",         { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "36_fishing_bay",     { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "37_caucasus",        { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "38_mannerheim_line", { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "39_crimea",          { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "42_north_america",   { std::make_tuple(-400, 400), std::make_tuple(-400, 400) } },
        { "44_north_america",   { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "45_north_america",   { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "47_canada_a",        { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } },
        { "51_asia",            { std::make_tuple(-500, 500), std::make_tuple(-500, 500) } }
    };

    auto boundaries = map_boundaries[game_info.map_name];

    string path = "maps/no-border/" + game_info.map_name + "_" + game_info.game_mode + ".png";

    map_info = {boundaries, path};

    return true;
}

int main(int argc, const char * argv[])
{
#ifdef DEBUG
    chdir("/Users/jantemmerman/Development/wotreplays");
#endif
    // std::exit(1);
    // process_reference_data();
    
    // std::exit(1);
    // string replay_file_name("/Users/jantemmerman/Development/wotreplays/20120707_2059_germany-E-75_himmelsdorf.wotreplay");
    // string replay_file_name("/Users/jantemmerman/Development/wotreplays/siegfried_line/20120628_2039_germany-E-75_siegfried_line.wotreplay");
    string replay_file_name = "/Users/jantemmerman/Development/wotreplays/replay-data/20120815_0309_germany-E-75_02_malinovka.wotreplay";
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

    // display_packet_summary(packets);
    // display_boundaries(game_info, packets);

    for (auto packet : replay.get_packets()) {
        if (packet.has_property(property::player_id)) {
            if (packet.player_id() == 77686001) {
                if (packet.has_property(property::health)) {
                   std::cout << packet.health() << "\n";
                }
                // print_packet(packet.get_data());
            }
        }
    }

    write_parts_to_file(replay);
    
    map_info map_info;
    if (get_map_info(game_info, map_info)) {
        png_bytepp row_pointers;
        int width, height, channels;
        read_png(map_info.mini_map, row_pointers, width, height, channels);
        bool alpha = channels == 4;
        create_image(row_pointers, width, height, replay, game_info, map_info, alpha);
        write_png("out/replay.png", row_pointers, width, height, alpha);
    }

    return EXIT_SUCCESS;
}