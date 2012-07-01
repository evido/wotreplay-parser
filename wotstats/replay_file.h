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

#include <boost/range/iterator_range.hpp>

namespace wotstats {
    typedef std::vector<char> buffer_t;
    typedef boost::iterator_range<buffer_t::iterator> slice_t;
    
    class replay_file {
    public:
        replay_file(std::istream &is);
        const buffer_t &get_game_begin() const;
        const buffer_t &get_game_end() const;
        const buffer_t &get_replay() const;
    private:
        void parse(buffer_t &buffer);
        void get_data_blocks(buffer_t &buffer, std::vector<slice_t> &data_blocks) const;
        uint32_t get_data_block_count(const buffer_t &buffer) const;
        void decrypt_replay(buffer_t &slice, const unsigned char* key);
        void extract_replay(buffer_t &compressed_replay, buffer_t &replay);
        buffer_t game_begin;
        buffer_t game_end;
        buffer_t replay;
    };
}

#endif
