 //
//  ReplayFile.cpp
//  wotstats
//
//  Created by Jan Temmerman on 01/07/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "replay_file.h"

#include <sstream>

#include <openssl/blowfish.h>
#include <zlib.h>

using namespace wotstats;

using std::ios;
using std::istream;
using std::vector;

#ifdef DEBUG

#include <fstream>
#include <string>

using std::string;
using std::ofstream;
using std::ios;

template <typename T>
static void debug_stream_content(const string &file_name, T begin, T end) {
    ofstream os(file_name, ios::binary | ios::ate);
    std::copy(begin, end, std::ostream_iterator<typename std::iterator_traits<T>::value_type>(os));
    os.close();
}

#else

// no-op
#define debug_stream_content(...)

#endif

const buffer_t &replay_file::get_game_begin() const {
    return game_begin;
}

const buffer_t &replay_file::get_game_end() const {
    return game_end;
}

const buffer_t &replay_file::get_replay() const {
    return replay;
}

static size_t get_size(istream& is) {
    is.seekg(0, ios::end);
    ios::pos_type size = is.tellg();
    is.seekg(0, ios::beg);
    return static_cast<size_t>(size);
}

static void read(istream& is, buffer_t &buffer) {
    ios::pos_type size = get_size(is);
    buffer.resize(size);
    is.read(&buffer[0], size);
}

replay_file::replay_file(istream &is) {
    // fully read file into buffer
    buffer_t buffer;
    read(is, buffer);
    parse(buffer);
}

void replay_file::parse(buffer_t &buffer) {
    // determine number of data blocks
    std::vector<slice_t> data_blocks;
    get_data_blocks(buffer, data_blocks);
    
    buffer_t raw_replay;
    
    switch(data_blocks.size()) {
    case 3:
            // extra data block is the game result
            game_end.resize(data_blocks[1].size());
            std::copy(data_blocks[1].begin(), data_blocks[1].end(), game_end.begin());
    case 2:
            // data blocks contain at least game begin state and replay data
            game_begin.resize(data_blocks[0].size());
            std::copy(data_blocks[0].begin(), data_blocks[0].end(), game_begin.begin());
            
            raw_replay.resize(data_blocks[2].size());
            std::copy(data_blocks[2].begin(), data_blocks[2].end(), raw_replay.begin());
            
            break;
    default:
            std::stringstream msg;
            msg << __func__
                << "Unexpected number of data blocks ("
                << data_blocks.size()
                << ") !\n";
            throw std::runtime_error(msg.str());
    }
    
    const unsigned char key[] = { 0xDE, 0x72, 0xBE, 0xA0, 0xDE, 0x04, 0xBE, 0xB1, 0xDE, 0xFE, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF};
    
    decrypt_replay(raw_replay, key);
    extract_replay(raw_replay, replay);
}

void replay_file::decrypt_replay(buffer_t &replay_data, const unsigned char *key_data) {
    debug_stream_content("/Users/jantemmerman/wotreplays/replay-ec.dat", replay_data.begin(), replay_data.end());
    
    BF_KEY key = {0};
    BF_set_key(&key, 16, key_data);
    
    const int block_size = 8;
    
    size_t padding_size = (block_size - (replay_data.size() % block_size));
    if (padding_size != 0) {
//        std::cout << replay_data.size()<< std::endl;
        size_t required_size = replay_data.size() + padding_size;
        replay_data.resize(required_size, 0);
//        std::cout << replay_data.size()<< std::endl;
    }
    
    unsigned char previous[block_size] = {0};
    for (auto it = replay_data.begin(); it != replay_data.end(); it += block_size) {
        unsigned char decrypted[block_size] = { 0 };
        BF_ecb_encrypt(reinterpret_cast<unsigned char*>(&(*it)), decrypted, &key, BF_DECRYPT);
        std::transform(previous, previous + block_size, decrypted, decrypted, std::bit_xor<unsigned char>());
        std::copy_n(decrypted, block_size, previous);
        std::copy_n(decrypted, block_size, reinterpret_cast<unsigned char*>(&(*it)));
    }
    
    if (padding_size != 0) {
        size_t original_size = replay_data.size() - padding_size;
        replay_data.resize(original_size, 0);
//        std::cout << replay_data.size()<< std::endl;
    }
}

uint32_t replay_file::get_data_block_count(const buffer_t &buffer) const {
    // number of data blocks is contained in an 'unsigned long' (4 bytes) 
    // with an offset of 4 bytes from the beginning of te file
    const size_t db_cnt_offset = 4;
    const uint32_t *db_cnt = reinterpret_cast<const uint32_t*>(&buffer[db_cnt_offset]);
    return *db_cnt;
} 

void replay_file::extract_replay(buffer_t &compressed_replay, buffer_t &replay) {
    debug_stream_content("/Users/jantemmerman/wotreplays/replay-c.dat", compressed_replay.begin(), compressed_replay.end());
    
    z_stream strm = { 
        .next_in  = reinterpret_cast<unsigned char*>(&(compressed_replay[0])),
        .avail_in = static_cast<uInt>(compressed_replay.size())
    };
    
    int ret = inflateInit(&strm);
    
    if (ret != Z_OK) {
        std::stringstream msg;
        msg << __func__ 
            << ": inflateInit() failed!";
        throw std::runtime_error(msg.str());
    }
    
    const int chunk = 1048576;
    std::unique_ptr<unsigned char[]> out(new unsigned char[chunk]);
    
    do {
        strm.avail_out = chunk;
        strm.next_out = out.get();
        ret = inflate(&strm, Z_NO_FLUSH);
        
        assert(ret != Z_STREAM_ERROR); 
        switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;    
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
        }
        
        int have = chunk - strm.avail_out;
        replay.resize(replay.size() + have);
        std::copy_n(out.get(), have, replay.end() - have);
        
    } while (strm.avail_out == 0);
    
    (void)inflateEnd(&strm);
    
    if ( ret != Z_STREAM_END ) {
        std::stringstream msg;
        msg << __func__ 
            << ": infate() failed!\n";
        throw std::runtime_error(msg.str());
    }
}

void replay_file::get_data_blocks(buffer_t &buffer, vector<slice_t> &data_blocks) const {
    // determine number of data blocks
    uint32_t nr_data_blocks = get_data_block_count(buffer);
    
    // reserve enough place in data_blocks vector
    data_blocks.reserve(nr_data_blocks);
    
    // try to create a data block reference for each data block
    uint32_t data_block_sz_offset = 8;
    for (uint32_t ix = 0; ix < nr_data_blocks; ++ix) {
        // create data block
        const uint32_t *block_size = reinterpret_cast<const uint32_t*>(&buffer[data_block_sz_offset]);
        size_t data_block_offset = data_block_sz_offset + sizeof(uint32_t);
        auto data_block_beg = buffer.begin() + data_block_offset;
        auto data_block_end = data_block_beg + *block_size;
        data_blocks.emplace_back(data_block_beg, data_block_end);
        
        // modify offset for next data block
        data_block_sz_offset = data_block_offset + *block_size;
    }
    
    // last slice contains encrypted / compressed game replay, seperated by 8 bytes with unknown content
    auto &last_data_block = data_blocks.back();
    data_blocks.emplace_back(last_data_block.end() + 8, buffer.end());
}