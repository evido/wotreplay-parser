/*
 Copyright (c) 2012, Jan Temmerman
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the <organization> nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL Jan Temmerman BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "json/json.h"
#include "parser.h"

#include <boost/lexical_cast.hpp>
#include <fstream>
#include <map>
#include <ostream>
#include <openssl/blowfish.h>
#include <sstream>
#include <string>
#include <type_traits>
#include <zlib.h>

using namespace wotreplay;
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


const buffer_t &parser::get_game_begin() const {
    return game_begin;
}

const buffer_t &parser::get_game_end() const {
    return game_end;
}


const buffer_t &parser::get_replay() const {
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

parser::parser(std::istream &is, bool debug)
    : debug(debug){
    // fully read file into buffer
    buffer_t buffer;
    read(is, buffer);
    parse(buffer);
}

void parser::set_debug(bool debug) {
    this->debug = debug;
}

bool parser::get_debug() const {
    return debug;
}

const std::string &parser::get_version() const {
    return version;
}


bool parser::is_legacy_replay(const buffer_t &buffer) const {
    return buffer.size() >= 10 && buffer[8] == 0x78 && buffer[9] == 0xDA;
}

bool parser::is_legacy() const {
    return legacy;
}
void parser::parse(buffer_t &buffer) {
    // determine number of data blocks
    std::vector<slice_t> data_blocks;
    this->legacy = is_legacy_replay(buffer);

    if (is_legacy()) {
        buffer_t raw_replay;
        
        raw_replay.resize(buffer.size() - 8);
        std::copy(buffer.begin() + 8, buffer.end(), raw_replay.begin());

        extract_replay(raw_replay, replay);
        read_packets();// no game info by default available
    } else {
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
        read_game_info();

        // read version string
        uint32_t version_string_sz = get_field<uint32_t>(replay.begin(), replay.end(), 12);
        version.assign(replay.begin() + 16, replay.begin() + 16 + version_string_sz);
    }
}

void parser::decrypt_replay(buffer_t &replay_data, const unsigned char *key_data) {
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

uint32_t parser::get_data_block_count(const buffer_t &buffer) const {
    // number of data blocks is contained in an 'unsigned long' (4 bytes) 
    // with an offset of 4 bytes from the beginning of te file
    const size_t db_cnt_offset = 4;
    const uint32_t *db_cnt = reinterpret_cast<const uint32_t*>(&buffer[db_cnt_offset]);
    return *db_cnt;
} 

void parser::extract_replay(buffer_t &compressed_replay, buffer_t &replay) {
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

void parser::get_data_blocks(buffer_t &buffer, vector<slice_t> &data_blocks) const {
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

static std::map<uint8_t, int> packet_lengths = {
    {0x03, 24},
    {0x04, 16},
    {0x05, 54},
    {0x07, 24},
    {0x08, 24},
    {0x0A, 61},
    {0x0B, 30},
    {0x0E, 25}, // changed {0x0E, 4}
    {0x0C,  3},
    {0x11, 12},
    {0x12, 16},
    {0x13, 16},
    {0x14,  4},
    {0x15, 44},
    {0x16, 52},
    {0x17, 16},
    {0x18, 16},
    {0x19, 16},
    {0x1B, 16},
    {0x1C, 20},
    {0x1D, 21},
    {0x1A, 16},
    {0x1E, 16},  // modified for 0.7.2 {0x1e, 160},
    {0x20, 21},  // modified for 0.8.0 {0x20, 4}
    {0x31,  4},  // indication of restart of the replay, probably not a part of the replay but necessary because of wrong detection of the start of the replay
    {0x0D, 22},
    {0x00, 22}
};

size_t parser::read_packets() {
    buffer_t &buffer = this->replay;

    if (is_legacy()) {
        packet_lengths[0x16] = 44;
    }

    size_t offset = 0;

    std::array<uint8_t, 6> marker = {0x2C, 0x01, 0x01, 0x00, 0x00, 0x00};

    auto pos = std::search(buffer.begin(), buffer.end(),marker.begin(), marker.end());
    if (pos != buffer.end()) {
        offset = (pos - buffer.begin()) + marker.size();
        if (debug) {
            std::cerr << "OFFSET: " << offset << "\n";
        }
    }

    size_t ix = offset;
    while (ix < buffer.size()) {
    
        if (packet_lengths.count(buffer[ix + 1]) < 1) {
            size_t count = packets.size();
            if (count < 500) {
                ix = static_cast<int>(++offset);
                if (debug) {
                    std::cerr << ix << "\n";
                    std::cerr << "WARNING INCORRECT OFFSET!\n";
                }
                continue;
            } else {
                if (debug) {
                    const packet_t &last_packet = packets.back();
                    const slice_t &packet_data = last_packet.get_data();
                    size_t packet_size = packet_data.size();
                    size_t prev_ix = ix - packet_size;
                    int unread = (int) buffer.size() - (int) ix;
                    std::cerr << "Bytes read: " << ix << std::endl
                        << "Packets read: " << count << std::endl
                        << "Last packets start: " << prev_ix << std::endl
                        << "Bytes unread: " << unread << std::endl
                        << "Bytes skipped: " << offset << std::endl;
                    if (unread > 25) {
                        std::cerr << "Unexpected end of the replay.\n";
                    }
                }
                break;
            }
        }

        size_t packet_length = packet_lengths[buffer[ix + 1]];
        
        switch(buffer[ix + 1]) {
            case 0x07:
            case 0x08: {
                packet_length += get_field<uint16_t>(buffer.begin(), buffer.end(), ix + 17);
                break;
            }
            case 0x17: {
                packet_length += get_field<uint8_t>(buffer.begin(), buffer.end(), ix + 9);
                break;
            }
            case 0x05: {
                packet_length += get_field<uint8_t>(buffer.begin(), buffer.end(), ix + 47);
                break;
            }
            case 0x20: {
                packet_length += get_field<uint8_t>(buffer.begin(), buffer.end(), ix + 14);
                break;
            }
            case 0x0B: {
                packet_length += get_field<uint8_t>(buffer.begin(), buffer.end(), ix + 23);
                break;
            }
            case 0x0e: {
                packet_length += get_field<uint32_t>(buffer.begin(), buffer.end(), ix + 10);
                break;
            }
            case 0x0D:
            case 0x00: {
                packet_length += get_field<uint32_t>(buffer.begin(), buffer.end(), ix + 15);
                break;
            }
        }
        
        auto packet_begin = buffer.begin() + ix;

        if (ix + packet_length < buffer.size()) {
            auto packet_end = packet_begin + packet_length;
            packets.emplace_back(parser::read_packet(packet_begin, packet_end));
        } else if (debug) {
            std::cerr << "Packet went out of replay file bounds.\n";
        }

        if (debug) {
              std::cerr << (int) buffer[ix + 1] << " " << ix << " " << packet_length << "\n";
        }
        
        ix += packet_length;
    }

    return ix;
}

const std::vector<packet_t> &parser::get_packets() const {
    return packets;
}

bool parser::find_property(uint32_t clock, uint32_t player_id, property property, packet_t &out) const
{
    // inline function function for using with stl to finding the range with the same clock
    auto has_same_clock = [&](const packet_t &target) -> bool  {
        // packets without clock are included
        return target.has_property(property::clock)
          && target.clock() == clock;
    };

    
    // find first packet with same clock
    auto it_clock_begin = std::find_if(packets.cbegin(), packets.cend(), has_same_clock);
    // find last packet with same clock
    auto it_clock_end = std::find_if_not(it_clock_begin, packets.cend(), [&](const packet_t &target) -> bool  {
        // packets without clock are included
        return !target.has_property(property::clock)
        || target.clock() == clock;
    });
    
    auto is_related_with_property = [&](const packet_t &target) -> bool {
        return target.has_property(property::clock) &&
        target.has_property(property::player_id) &&
        target.has_property(property) &&
        target.player_id() == player_id;
    };

    auto it = std::find_if(it_clock_begin, it_clock_end, is_related_with_property);
    bool found = it != it_clock_end;
    
    if (!found) {
        auto result_after = std::find_if(it_clock_end, packets.end(), is_related_with_property);
        auto it_clock_rbegin = packets.crbegin() + (packets.size() - (it_clock_begin - packets.begin()));
        auto result_before = std::find_if(it_clock_rbegin, packets.crend(), is_related_with_property);

        if(result_after != packets.cend() && result_before != packets.crend()) {
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

bool parser::find_property(size_t packet_id, property property, packet_t &out) const
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

const game_info &parser::get_game_info() const {
    return game_info;
}

static std::map<std::string, std::array<std::array<int, 2>,2>> map_boundaries = {
    { "01_karelia",         {-500, 500, -500, 500} },
    { "02_malinovka",       {-500, 500, -500, 500} },
    { "03_campania",        {-300, 300, -300, 300} },
    { "04_himmelsdorf",     {-300, 400, -300, 400} },
    { "05_prohorovka", 	    {-500, 500, -500, 500} },
    { "06_ensk",            {-300, 300, -300, 300} },
    { "07_lakeville",       {-400, 400, -400, 400} },
    { "08_ruinberg",        {-400, 400, -400, 400} },
    { "10_hills",           {-400, 400, -400, 400} },
    { "11_murovanka",       {-400, 400, -400, 400} },
    { "13_erlenberg",       {-500, 500, -500, 500} },
    { "14_siegfried_line",  {-500, 500, -500, 500} },
    { "15_komarin",         {-400, 400, -400, 400} },
    { "17_munchen",         {-300, 300, -300, 300} },
    { "18_cliff",           {-500, 500, -500, 500} },
    { "19_monastery",       {-500, 500, -500, 500} },
    { "22_slough",          {-500, 500, -500, 500} },
    { "23_westfeld",        {-500, 500, -500, 500} },
    { "28_desert",          {-500, 500, -500, 500} },
    { "29_el_hallouf",      {-500, 500, -500, 500} },
    { "31_airfield",        {-500, 500, -500, 500} },
    { "33_fjord",           {-500, 500, -500, 500} },
    { "34_redshire",        {-500, 500, -500, 500} },
    { "35_steppes",         {-500, 500, -500, 500} },
    { "36_fishing_bay",     {-500, 500, -500, 500} },
    { "37_caucasus",        {-500, 500, -500, 500} },
    { "38_mannerheim_line", {-500, 500, -500, 500} },
    { "39_crimea",          {-500, 500, -500, 500} },
    { "42_north_america",   {-400, 400, -400, 400} },
    { "44_north_america",   {-500, 500, -500, 500} },
    { "45_north_america",   {-500, 500, -500, 500} },
    { "47_canada_a",        {-500, 500, -500, 500} },
    { "51_asia",            {-500, 500, -500, 500} }
};

void parser::read_game_info() {
    // get game details
    Json::Value root;
    Json::Reader reader;
    std::string doc(game_begin.begin(), game_begin.end());
    reader.parse(doc, root);
    auto vehicles = root["vehicles"];

    auto player_name = root["playerName"].asString();


    for (auto it = vehicles.begin(); it != vehicles.end(); ++it) {
        unsigned player_id = boost::lexical_cast<int>(it.key().asString());
        std::string name = (*it)["name"].asString();
        if (name == player_name) {
            game_info.recorder_id = player_id;
        }
        int team_id = (*it)["team"].asInt();
        game_info.teams[team_id - 1].insert(player_id);
    }

    game_info.map_name = root["mapName"].asString();

    if (root.isMember("gameplayType")) {
        game_info.game_mode = root["gameplayType"].asString();
    } else {
        // from 8.0 this is renamed
        game_info.game_mode = root["gameplayID"].asString();
    }
    
    game_info.game_mode.resize(3);

    // explicit check for game version should be better
    if (map_boundaries.find(game_info.map_name) == map_boundaries.end()) {

        if (game_info.map_name == "north_america") {
            game_info.map_name = "44_north_america";
        } else {
            for (auto entry : map_boundaries) {
                string map_name = entry.first;
                string short_map_name(map_name.begin() + 3, map_name.end());
                if (short_map_name == game_info.map_name) {
                    // rewrite map name
                    game_info.map_name = map_name;
                }
            }
        }
    }

    game_info.boundaries = map_boundaries[game_info.map_name];
    game_info.mini_map = "maps/no-border/" + game_info.map_name + "_" + game_info.game_mode + ".png";
}

void wotreplay::show_packet_summary(const std::vector<packet_t>& packets) {
    std::map<char, int> packet_type_count;

    for (const packet_t &p : packets) {
        packet_type_count[p.type()]++;
    }

    for (auto it : packet_type_count) {
        printf("packet_type [%02x] = %d\n", it.first, it.second);
    }

    printf("Total packets = %lu\n", packets.size());
}

void wotreplay::write_parts_to_file(const parser &replay) {
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

void wotreplay::show_map_boundaries(const game_info &game_info, const std::vector<packet_t> &packets) {

    float min_x = 0.f, min_y = 0.f, max_x = 0.f, max_y = 0.f;

    for (const packet_t &packet : packets) {
        if (packet.type() != 0xa) {
            continue;
        }

        uint32_t player_id = packet.player_id();

        const auto &teams = game_info.teams;
        for (auto it = teams.begin(); it != teams.end(); ++it) {
            // only take into account positions of 'valid' players.
            if (it->find(player_id) != it->end()) {
                // update the boundaries
                auto position = packet.position();
                min_x = std::min(min_x, std::get<0>(position));
                min_y = std::min(min_y, std::get<2>(position));
                max_x = std::max(max_x, std::get<0>(position));
                max_y = std::max(max_y, std::get<2>(position));
                break;
            }
        }
    }

    printf("The boundaries of used positions in this replay are: min_x = %f max_x = %f min_y = %f max_y = %f\n", min_x, max_x, min_y, max_y);
}





