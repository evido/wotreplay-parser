//
//  ReplayFile.h
//  wotstats
//
//  Created by Jan Temmerman on 01/07/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef wotstats_ReplayFile_h
#define wotstats_ReplayFile_h

#include <iostream>
#include <vector>
#include <algorithm>
#include <set>

#include "types.h"
#include "packet.h"

namespace wotstats {
    struct game_info {
        // recorder client information
        std::string version;

        // game information
        std::string map_name;
        std::string game_mode;
        std::array<std::set<int>, 2> teams;

        // player information
        unsigned recorder_id;

        // map information
        std::array<std::tuple<int, int>, 2> boundaries;
        std::string mini_map;
    };

    class replay_file {
    public:
        replay_file() = default;
        replay_file(std::istream &is);
        const std::string& get_version() const;
        const buffer_t &get_game_begin() const;
        const buffer_t &get_game_end() const;
        const buffer_t &get_replay() const;
        const game_info &get_game_info() const;
        const std::vector<packet_t> &get_packets() const;
        bool find_property(size_t packet_id, property property, packet_t &out) const;
        bool find_property(uint32_t clock, uint32_t player_id, property property, packet_t &out) const;
        // helper method responsible for parsing file
        void parse(buffer_t &buffer);
    private:
        // process raw replay file
        void get_data_blocks(buffer_t &buffer, std::vector<slice_t> &data_blocks) const;
        uint32_t get_data_block_count(const buffer_t &buffer) const;
        void decrypt_replay(buffer_t &slice, const unsigned char* key);
        void extract_replay(buffer_t &compressed_replay, buffer_t &replay);
        // methods helping with packet processing
        template <typename iterator>
        packet_t read_packet(iterator begin, iterator end);
        size_t read_packets();
        void read_game_info();
        // data members
        buffer_t game_begin;
        buffer_t game_end;
        buffer_t replay;
        std::vector<packet_t> packets;
        std::string version;
        game_info game_info;
    };

    template <typename iterator>
    packet_t replay_file::read_packet(iterator begin, iterator end) {
        return packet_t(boost::make_iterator_range(begin, end));
    }
}

#endif
