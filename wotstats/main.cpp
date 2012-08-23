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


void user_error_fn(png_structp png_ptr, png_const_charp charp) {
    std::cerr << charp << "\n";
}

void draw_routes(int player_id, std::vector<wotstats::packet_t> &packets) {
    FILE *fp = fopen("/Users/jantemmerman/Development/wotreplays/replay.png", "wb");
    // check fp
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING ,nullptr, user_error_fn, user_error_fn);
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