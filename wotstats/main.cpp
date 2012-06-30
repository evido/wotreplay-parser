//
//  main.cpp
//  wotstats
//
//  Created by Jan Temmerman on 14/05/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

#include <boost/range/iterator_range.hpp>

#include <openssl/blowfish.h>
#include <zlib.h>

using std::ifstream;
using std::ofstream;
using std::ios;
using std::endl;
using std::cout;
using std::vector;
using std::ostream_iterator;
using boost::iterator_range;

typedef vector<char> buffer_t;
typedef iterator_range<buffer_t::iterator> data_block_t;

ios::pos_type get_size(std::istream& is) {
    is.seekg(0, ios::end);
    ios::pos_type size = is.tellg();
    is.seekg(0, ios::beg);
    return size;
}

void read(std::istream& is, buffer_t &buffer) {
    ios::pos_type size = get_size(is);
    buffer.resize(size);
    is.read(&buffer[0], size);
}

uint32_t number_of_data_blocks(buffer_t &buffer) {
    // number of data blocks is contained in an 'unsigned long' (4 bytes) 
    // with an offset of 4 bytes from the beginning of te file
    const size_t offset = 4;
    uint32_t *nr_data_blocks = reinterpret_cast<uint32_t*>(&buffer[0] + offset);
    return *nr_data_blocks;
}


void get_data_blocks(buffer_t &buffer, vector<data_block_t> &data_blocks) {
    // determine number of data blocks
    uint32_t nr_data_blocks = number_of_data_blocks(buffer);
    
    // reserve enough place in data_blocks vector
    data_blocks.reserve(nr_data_blocks);
    
    // try to create a data block reference for each data block
    uint32_t data_block_sz_offset = 8;
    for (uint32_t ix = 0; ix < nr_data_blocks; ++ix) {
        // create data block
        uint32_t *block_size = reinterpret_cast<uint32_t*>(&buffer[0] + data_block_sz_offset);
        size_t data_block_offset = data_block_sz_offset + sizeof(uint32_t);
        auto data_block_beg = buffer.begin() + data_block_offset;
        auto data_block_end = data_block_beg + *block_size;
        data_blocks.emplace_back(data_block_beg, data_block_end);
        
        // modify offset for next data block
        data_block_sz_offset = data_block_offset + *block_size;
    }
}

int main(int argc, const char * argv[])
{
    std::string replay_file_name("/Users/jantemmerman/wotreplays/20120628_2039_germany-E-75_siegfried_line.wotreplay");
    cout << "Reading replay file: " << replay_file_name << "\n";
    
    // 0. read replay file fully into buffer
    ifstream is(replay_file_name, std::ios::binary);
    buffer_t buffer;
    read(is, buffer);
    is.close();
    
    // 1. determine number of data blocks
    uint32_t nr_data_blocks = number_of_data_blocks(buffer);
    std::cout << "Number of data blocks in replay file: " << nr_data_blocks << "\n";
    
    
    // 2. split the replay buffer into data blocks
    vector<data_block_t> data_blocks;
    get_data_blocks(buffer, data_blocks);
    
    cout << "\n";
    for (auto it = data_blocks.begin(); it != data_blocks.end(); ++it) {
        cout << "Block Index: " << (it - data_blocks.begin()) << "\n";
        cout << "Block Size: " << it->size() << "\n";
        cout << "Block Content:\n";
        std::copy(it->begin(), it->end(), ostream_iterator<char>(cout));
        cout << "\n\n";
    }
    
    // 3. rest is the replay data, encrypted + compressed
    auto &last_data_block = data_blocks.back();
    data_block_t replay_data = boost::make_iterator_range(last_data_block.end() + 8, buffer.end());
    
    ofstream os("/Users/jantemmerman/wotreplays/replay.dat", ios::binary | ios::ate);
    std::copy(replay_data.begin(), replay_data.end(), ostream_iterator<unsigned char>(os));
    cout << "Replay data size: " << replay_data.size() << "\n";
    os.close();
    
    ofstream decoded("/Users/jantemmerman/wotreplays/replay.decoded.dat", ios::binary | ios::ate);    
    BF_KEY key;
    const unsigned char key_data[] = { 0xDE, 0x72, 0xBE, 0xA0, 0xDE, 0x04, 0xBE, 0xB1, 0xDE, 0xFE, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF};
    BF_set_key(&key, 16, reinterpret_cast<const unsigned char *>(&key_data));
    
    // TODO: make sure replay data's size is divisible by 8
    const int block_size = 8;
    unsigned char previous[block_size] = {0};
    for (auto it = replay_data.begin(); it != replay_data.end(); it += block_size) {
        unsigned char decrypted[block_size] = { 0 };
        BF_ecb_encrypt(reinterpret_cast<unsigned char*>(&(*it)), decrypted, &key, BF_DECRYPT);
        std::transform(previous, previous + block_size, decrypted, decrypted, std::bit_xor<unsigned char>());
        std::copy_n(decrypted, block_size, previous);
        std::copy_n(decrypted, block_size, reinterpret_cast<unsigned char*>(&(*it)));
    }
    
    std::copy(replay_data.begin(), replay_data.end(), ostream_iterator<unsigned char>(decoded));
    
    decoded.close();
    
    // 4. unzip this file
    z_stream strm = { 
        .next_in = reinterpret_cast<unsigned char*>(&(replay_data[0])),
        .avail_in = static_cast<uInt>(replay_data.size())
    };
    
    int ret = inflateInit(&strm);
    
    if (ret != Z_OK) {
        std::cerr << "inflateInit() failed!\n";
        std::exit(1);
    }
    
    const int chunk = 16384;
    unsigned char out[chunk];
    
    vector<char> uncompressed_replay;
    
    do {
        strm.avail_out = chunk;
        strm.next_out = out;
        ret = inflate(&strm, Z_NO_FLUSH);
        
        assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
        switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
        }
        
        int have = chunk - strm.avail_out;
        uncompressed_replay.resize(uncompressed_replay.size() + have);
        std::copy_n(out, have, uncompressed_replay.end() - have);
            
    } while (strm.avail_out == 0);
    
    (void)inflateEnd(&strm);
    
    if ( (ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR) == Z_OK ) 
        std::cout << "We're still OK!\n";
    
    ofstream uncompressed("/Users/jantemmerman/wotreplays/replay.uncompressed.decoded.dat", ios::binary | ios::ate);
    std::copy(uncompressed_replay.begin(), uncompressed_replay.end(), ostream_iterator<char>(uncompressed));
    uncompressed.close();
    
    return 0;
}

