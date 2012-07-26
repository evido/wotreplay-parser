//
//  main.cpp
//  wotstats
//
//  Created by Jan Temmerman on 14/05/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "replay_file.h"

#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstdio>
#include <algorithm>


#include <boost/any.hpp>

#include <algorithm>
#include <functional>
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

class Test {
public:
    int get() const { return 2; }
    int get() { return 1; }
};


void read_packets(const buffer_t &buffer, size_t offset, vector<slice_t> &packets) {

}

void count_player_packets(const buffer_t &buffer, size_t offset) {
    
    std::map<char, int> packet_lengths = {
        {0x03, 24},
        {0x04, 4},
        {0x05, 54},
        {0x07, 12},
        {0x08, 12},
        {0x0A, 61},
        {0x0E, 4},
        {0x0C, 3},
        {0x12, 16},
        {0x13, 16},
        {0x14, 4},
        {0x15, 44},
        {0x16, 52},
        {0x17, 16},
        {0x18, 16},
        {0x19, 16},
        {0x1B, 12},
        {0x1C, 20},
        {0x1D, 21},
        {0x20, 4},
        {0x31, 4}
    };

    int ix = static_cast<int>(offset);
    int count = 0;
    
    
    while (ix < buffer.size()) {
        std::cout << "Position: "  << (ix + 1)  << " Value: " << (int) buffer[ix + 1] << std::endl;
        if (packet_lengths.find(buffer[ix + 1]) != packet_lengths.end()) {
            count++;
        } else {
            std::cout << "Could not continue at " << ix << std::endl;
            break;
        }
        switch(buffer[ix + 1]) {
            case 0x07:
            case 0x08: {
                int packet_length = packet_lengths[buffer[ix + 1]];
                packet_length = 24 + *reinterpret_cast<const unsigned short*>(&buffer[ix + 17]);
                std::cout << "0x0" << static_cast<int>(buffer[ix + 1]) <<  " packet length: " << packet_length << " code: " << (buffer[ix + packet_lengths[buffer[ix + 1]] + 1] & 0xFF) << std::endl;
                std::cout.flush();
                ix += packet_length; 
                break;
            }
            case 0x17: {
                int packet_length = packet_lengths[buffer[ix + 1]];
                packet_length += buffer[ix + 9];
                std::cout << "0x17" <<  " packet length: " << packet_length << " code: " << static_cast<int>(buffer[ix + 9]) << std::endl;
                std::cout.flush();
                ix += packet_length;
                break;
            }
            case 0x1B: {
                int packet_length = packet_lengths[buffer[ix + 1]];
                if (buffer[ix + packet_length + 1] != 0x31) packet_length += 4;
                std::cout << "0x1B packet length = " << packet_length << "\n";
                ix += packet_length;
                break;
            }
            case 0x04: {
                int packet_length = packet_lengths[buffer[ix + 1]];
                if (buffer[ix] < 0x10) packet_length = 16;
                std::cout << "0x04 packet length = " << packet_length << "\n";
                ix += packet_length;
                break;
            }
            case 0x05: {
                int packet_length = packet_lengths[buffer[ix + 1]];
                packet_length += buffer[ix + 47];
                std::cout << "0x05 packet length = " << packet_length << "\n";
                ix += packet_length;
                break;
            }
            case 0x0A: {
                int packet_length = packet_lengths[buffer[ix + 1]];
                std::cout << "0x0A packet length = " << packet_length << "\n";
                ix += packet_length;
                break;
            }
            default: {
                ix += packet_lengths[buffer[ix + 1]];
                break;
            }
        }
    }
    
    std::cout << "Read " << count << " packets." << std::endl;
}

int main(int argc, const char * argv[])
{
    string replay_file_name("/Users/jantemmerman/Development/wotreplays/20120707_2059_germany-E-75_himmelsdorf.wotreplay");

    ifstream is(replay_file_name, std::ios::binary);

    if (!is) {
        std::cerr << "Something went wrong with reading file: " << replay_file_name << std::endl;
        std::exit(-1);
    }

    replay_file replay(is);
    is.close();
    
    const buffer_t &replay_data = replay.get_replay();
    count_player_packets(replay_data, 25023);
    
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

    return 0; 
 }

