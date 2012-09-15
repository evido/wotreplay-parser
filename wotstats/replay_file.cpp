 //
//  ReplayFile.cpp
//  wotstats
//
//  Created by Jan Temmerman on 01/07/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "replay_file.h"

#include <sstream>
#include <map>
#include <openssl/blowfish.h>
#include <zlib.h>
#include <type_traits>
#include <fstream>
#include <string>

using namespace wotstats;
using namespace std;

#if DEBUG_REPLAY_FILE

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
    is.read(reinterpret_cast<char*>(&buffer[0]), size);
}

replay_file::replay_file(istream &is) {
    // fully read file into buffer
    buffer_t buffer;
    read(is, buffer);
    parse(buffer);
}

const std::string &replay_file::get_version() const {
    return version;
}

void replay_file::parse(buffer_t &buffer) {
    // determine number of data blocks
    std::vector<slice_t> data_blocks;
    get_data_blocks(buffer, data_blocks);
    
    buffer_t raw_replay;
    
    switch(data_blocks.size()) {
    case 3:
            game_end.resize(data_blocks[1].size());
            std::copy(data_blocks[1].begin(), data_blocks[1].end(), game_end.begin());

            game_begin.resize(data_blocks[0].size());
            std::copy(data_blocks[0].begin(), data_blocks[0].end(), game_begin.begin());

            raw_replay.resize(data_blocks[2].size());
            std::copy(data_blocks[2].begin(), data_blocks[2].end(), raw_replay.begin());
            break;
    case 2:
            // data blocks contain at least game begin state and replay data
            game_begin.resize(data_blocks[0].size());
            std::copy(data_blocks[0].begin(), data_blocks[0].end(), game_begin.begin());
            
            raw_replay.resize(data_blocks[1].size());
            std::copy(data_blocks[1].begin(), data_blocks[1].end(), raw_replay.begin());
            
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
    read_packets();
}

void replay_file::decrypt_replay(buffer_t &replay_data, const unsigned char *key_data) {
    debug_stream_content("out/replay-ec.dat", replay_data.begin(), replay_data.end());
    
    BF_KEY key = {0};
    BF_set_key(&key, 16, key_data);
    
    const int block_size = 8;
    
    size_t padding_size = (block_size - (replay_data.size() % block_size));
    if (padding_size != 0) {
        size_t required_size = replay_data.size() + padding_size;
        replay_data.resize(required_size, 0);
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
    debug_stream_content("out/replay-c.dat", compressed_replay.begin(), compressed_replay.end());

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
    unsigned long data_block_sz_offset = 8;
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

size_t replay_file::read_packets() {

    buffer_t &buffer = this->replay;
    
    std::map<uint8_t, int> packet_lengths = {
        {0x03, 24},
        {0x04, 16},
        {0x05, 54},
        {0x07, 24},
        {0x08, 24},
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
        {0x1B, 16},
        {0x1C, 20},
        {0x1D, 21},
        {0x1e, 160},
        {0x20, 4},
        {0x31, 4}
    };

    size_t offset = 0;

    std::vector<uint8_t> marker = {0x2C, 0x01, 0x01, 0x00, 0x00, 0x00};

    auto pos = std::search(buffer.begin(), buffer.end(),marker.begin(), marker.end());
    if (pos != buffer.end()) {
        offset = (pos - buffer.begin()) + marker.size();
#if DEBUG_REPLAY_FILE
        std::cerr << "OFFSET: " << offset << "\n";
#endif
    }

    size_t ix = offset;
    while (ix < buffer.size()) {

        if (packet_lengths.count(buffer[ix + 1]) < 1) {
            size_t count = packets.size();
            if (count < 500) {
                ix = static_cast<int>(++offset);
#if DEBUG_REPLAY_FILE
                std::cerr << ix << "\n";
                std::cerr << "WARNING INCORRECT OFFSET!\n";
#endif
                continue;
            } else {
#if DEBUG_REPLAY_FILE
                const packet_t &last_packet = packets.back();
                const slice_t &packet_data = last_packet.get_data();
                size_t packet_size = packet_data.size();
                size_t prev_ix = ix - packet_size;
                std::cerr << "Bytes read: " << ix << std::endl
                    << "Packets read: " << count << std::endl
                    << "Last packets start: " << prev_ix << std::endl
                    << "Bytes unread: " << buffer.size() - ix << std::endl
                    << "Bytes skipped: " << offset << std::endl;
#endif
                break;
            }
        }

        size_t packet_length;
        
        switch(buffer[ix + 1]) {
            case 0x07:
            case 0x08: {
                packet_length = packet_lengths[buffer[ix + 1]];
                packet_length += get_field<uint16_t>(buffer.begin(), buffer.end(), ix + 17);
                break;
            }
            case 0x17: {
                packet_length = packet_lengths[buffer[ix + 1]];
                packet_length += get_field<uint8_t>(buffer.begin(), buffer.end(), ix + 9);
                break;
            }
            case 0x05: {
                packet_length = packet_lengths[buffer[ix + 1]];
                packet_length += get_field<uint8_t>(buffer.begin(), buffer.end(), ix + 47);
                break;
            }
            default: {
                packet_length = packet_lengths[buffer[ix + 1]];
                break;
            }
        }
        
        auto packet_begin = buffer.begin() + ix;
        auto packet_end = packet_begin + packet_length;
        packets.emplace_back(replay_file::read_packet(packet_begin, packet_end));
        
        ix += packet_length;
    }
    
    return ix;
}


const std::vector<packet_t> &replay_file::get_packets() const {
    return packets;
}

bool replay_file::find_property(size_t packet_id, property property, packet_t &out) const
{
    auto packet = packets.begin() + packet_id;
    // method is useless if the packet does not have a clock or player_id
    assert(packet->has_property(property::clock) && packet->has_property(property::player_id));

    // inline function function for using with stl to finding the range with the same clock
    auto has_same_clock =
        [&packet](const packet_t &target) -> bool  {
            // packets without clock are included
            return !target.has_property(property::clock)
                || target.clock() == packet->clock();
        };
    
    // start searching from the next packet to the end for the first packet without the same clock
    auto it_clock_end = std::find_if_not(packets.cbegin() + packet_id + 1, packets.cend(), has_same_clock);
    // start searching from the previous packet to the beginning for the first packet without the same clock
    auto it_clock_rbegin = std::find_if_not(packets.crbegin() + (packets.size() - packet_id), packets.crend(), has_same_clock);
    // change it_clock_rbegin to forward iterator
    auto it_clock_begin = (it_clock_rbegin + 1).base();

    auto is_related_with_property = [&](const packet_t &target) -> bool {
        return target.has_property(property::clock) &&
        target.has_property(property::player_id) &&
        target.has_property(property) &&
        target.player_id() == packet->player_id() &&
        target.clock() ==  packet->clock();
    };
    
    auto it = std::find_if(it_clock_begin, it_clock_end, is_related_with_property);
    bool found = it != it_clock_end;

    if (!found) {
        auto result_after = std::find_if(it_clock_end, packets.end(), is_related_with_property);
        auto result_before = std::find_if(it_clock_rbegin, packets.crend(), is_related_with_property);

        if(result_after != packets.end() && result_before != packets.rend()) {
            // if both iterators point to items within the collection
            auto cmp_by_clock = [](const packet_t &left, const packet_t &right) -> int {
                return left.clock() <= right.clock();
            };
            out = std::min(*result_before, *result_after, cmp_by_clock);
            found = true;
        } else if (result_after == packets.end() && result_before != packets.rend()) {
            // only result_before points to item in collection
            out = *result_before;
            found = true;
        } else if (result_after != packets.end() && result_before == packets.rend()) {
            // only result_after points to item in collection
            out = *result_after;
            found = true;
        } 
    } else {
        out = *it;
    }

    return found;
}





