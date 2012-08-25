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
#include "json/json.h"

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

/*
 
 void decode_packet(const buffer_t &packet) {

    std::vector<float> f_packet((const  float*) (&packet[13]), (const  float*) &packet[61]);


    packet_t p {
        .clock = *(const unsigned*) &packet[5],
        .player_id = *(const unsigned*) &packet[9],
        .position = {
            *(const float*) &packet[21],
            *(const float*) &packet[25],
            *(const float*) &packet[29]
        }
    };


    // printf("%d %f %f %f\n", p.player_id, p.position[0], p.position[1], p.position[2]);
}
*/

double round(double number)
{
    return number < 0.0 ? ceil(number - 0.5) : floor(number + 0.5);
}

void user_error_fn(png_structp png_ptr, png_const_charp charp) {
    std::cerr << charp << "\n";
}


void display_packet_summary(const std::vector<packet_t>& packets) {
    std::map<char, int> packet_type_count;

    for (auto p : packets) {
        packet_type_count[p.type]++;
    }
    
    for (auto it : packet_type_count) {
        printf("packet_type [%02x] = %d\n", it.first, it.second);
    }
    printf("Total packets = %lu\n", packets.size());

}

std::tuple<float, float, float> get_position(const packet_t &packet) {
    float x = *(const float*) &packet.data[21];
    float y = *(const float*) &packet.data[25];
    float z = *(const float*) &packet.data[29];
    return std::make_tuple(x,y,z);
}

#include <cmath>

void create_image(unsigned char **image, const std::vector<packet_t> &packets) {
    /* x = horizontal, z = vertical, y = height*/
    for (auto packet : packets) {
        
        if (packet.type != 0xa) {
            continue;
        }

        int player_id = (*(const unsigned*) &packet.data[9]);
        auto position = get_position(packet);

        int y = (int) (512 - std::get<2>(position));
        int x = (int) (std::get<0>(position) + 512)*4;

        if (player_id == 90604923) {
            image[y][x + 1] = 0xff;
            image[y][x+3] = 0xff;
        } else if (player_id == 90604924) {
            image[y][x] = 0xff;
            image[y][x+3] = 0xff;
        }

        printf("x = %f, y = %f, z = %f\n", std::get<0>(position),
                std::get<1>(position), std::get<2>(position));
    }
}

void write_row_callback(png_structp, png_uint_32,
                        int pass)
{
    /* put your code here */
}

int main(int argc, const char * argv[])
{
    string replay_file_name("/Users/jantemmerman/Development/wotreplays/20120707_2059_germany-E-75_himmelsdorf.wotreplay");
    // string replay_file_name("/Users/jantemmerman/Development/wotreplays/siegfried_line/20120628_2039_germany-E-75_siegfried_line.wotreplay");
    ifstream is(replay_file_name, std::ios::binary);

    if (!is) {
        std::cerr << "Something went wrong with reading file: " << replay_file_name << std::endl;
        std::exit(-1);
    }
    
    replay_file replay(is);
    is.close();
    
    std::vector<packet_t> packets;
    replay.get_packets(packets);

    display_packet_summary(packets);
    int width = 1024, height = 1024, channel = 4;
    // unsigned char *image = new unsigned char[width*height*channel]();
    png_bytep row_pointers[height];
    for (int i = 0; i < height; ++i) {
        row_pointers[i] = new unsigned char[width*channel]();
        std::fill((int*) row_pointers[i], (int*) row_pointers[i] + 1024, 0xff000000);
    }
    create_image(row_pointers, packets);
    FILE *fp = fopen("/Users/jantemmerman/Development/wotreplays/replay.png", "wb");
    // check fp
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING ,nullptr, user_error_fn, user_error_fn);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        std::cerr << "error\n";
    }

    png_init_io(png_ptr, fp);

    // auto write_row_callback = [](png_structp, png_uint_32, int){};
    png_set_write_status_fn(png_ptr, write_row_callback);

    png_set_filter(png_ptr, 0,PNG_FILTER_VALUE_NONE);
    
    png_set_IHDR(png_ptr, info_ptr, 1024, 1024,
                 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    std::cout << png_get_rowbytes(png_ptr, info_ptr);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    fclose(fp);
    // delete []image;
//    ofstream game_begin("/Users/jantemmerman/wotreplays/game_begin.txt", ios::binary | ios::ate);
//    std::copy(replay.get_game_begin().begin(),
//              replay.get_game_begin().end(),
//              ostream_iterator<char>(game_begin));
//    game_begin.close();
//    
//    
//    
//    ofstream game_end("/Users/jantemmerman/wotreplays/game_end.txt", ios::binary | ios::ate);
//    std::copy(replay.get_game_end().begin(),
//              replay.get_game_end().end(),
//              ostream_iterator<char>(game_end));
//    game_end.close();
//    
//    ofstream replay_content("/Users/jantemmerman/wotreplays/replay.dat", ios::binary | ios::ate);
//    std::copy(replay.get_replay().begin(),
//              replay.get_replay().end(),
//              ostream_iterator<char>(replay_content));
//    replay_content.close();
    
    return EXIT_SUCCESS;
}