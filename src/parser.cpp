#include "json/json.h"

#include "arena.h"
#include "logger.h"
#include "packet_reader.h"
#include "packet_reader_80.h"
#include "parser.h"
#include "regex.h"
#include "tank.h"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <map>
#include <memory>
#include <openssl/blowfish.h>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <zlib.h>

using namespace wotreplay;
using namespace boost::filesystem;

#if DEBUG_REPLAY_FILE

template <typename T>
static void debug_stream_content(const std::string &file_name, T begin, T end) {
    std::ofstream os(file_name, std::ios::binary | std::ios::ate);
    std::copy(begin, end, std::ostream_iterator<typename std::iterator_traits<T>::value_type>(os));
    os.close();
}

#else

// no-op
#define debug_stream_content(...)

#endif

parser_t::parser_t(load_data_mode_t load_data_mode, bool debug)
    : debug(debug), load_data_mode(load_data_mode)
{
    // empty
    if (load_data_mode == load_data_mode_t::bulk) {
        this->load_data();
    }
}

void parser_t::parse(std::istream &is, wotreplay::game_t &game) {
    buffer_t buffer((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    parse(buffer, game);
}

void parser_t::parse(buffer_t &buffer, wotreplay::game_t &game) {
    // determine number of data blocks
    std::vector<slice_t> data_blocks;
    buffer_t raw_replay;
    this->legacy = is_legacy_replay(buffer);

    if (is_legacy()) {
        raw_replay.resize(buffer.size() - 8);
        std::copy(buffer.begin() + 8, buffer.end(), raw_replay.begin());

        // no decryption necessary
        // no game info available
    } else {
        get_data_blocks(buffer, data_blocks);

        if (debug) {
            for (int i = 0; i < data_blocks.size(); ++i) {
                debug_stream_content((boost::format("out/data-block-%1%.dat") % i).str(),
                                    data_blocks[i].begin(), data_blocks[i].end());
            }
        }
        
        if (data_blocks.size() < 2) {
            std::string message((boost::format("Unexpected number of data blocks (%1%).") % data_blocks.size()).str());
            throw std::runtime_error(message);
        }

        buffer_t &game_begin = game.game_begin;
        game_begin.resize(data_blocks[0].size());
        std::copy(data_blocks[0].begin(), data_blocks[0].end(), game_begin.begin());

        switch(data_blocks.size()) {
            case 4: {
                buffer_t &game_end = game.game_end;
                game_end.resize(data_blocks[1].size());
                std::copy(data_blocks[1].begin(), data_blocks[1].end(), game_end.begin());
            }
            case 3: {
                // third block contains game summary
            }
        }

        raw_replay.resize(data_blocks.back().size());
        std::copy(data_blocks.back().begin(), data_blocks.back().end(), raw_replay.begin());

        static const unsigned char key[] = {
            0xDE, 0x72, 0xBE, 0xA0,
            0xDE, 0x04, 0xBE, 0xB1,
            0xDE, 0xFE, 0xBE, 0xEF,
            0xDE, 0xAD, 0xBE, 0xEF
        };
        
        decrypt_replay(raw_replay, key);
        read_game_info(game);
    }

    extract_replay(raw_replay, game.replay);
    
    // read version string
    uint32_t version_string_sz = get_field<uint32_t>(game.replay.begin(), game.replay.end(), 12);
    std::string version(game.replay.begin() + 16, game.replay.begin() + 16 + version_string_sz);
    game.version = version_t(version);
    
    if (!this->setup(game.version)) {
        logger.writef(log_level_t::warning, "Warning: Replay version (%1%) not marked as compatible.\n", game.version.text);
    }

    if (debug) {
        debug_stream_content("out/replay.dat", game.replay.begin(), game.replay.end());
    }

    read_packets(game);

    if (debug) {
        show_packet_summary(game.get_packets());
    }
}

bool parser_t::setup(const version_t &version) {
    this->packet_reader = std::unique_ptr<packet_reader_t>(new packet_reader_80_t());
    return packet_reader->is_compatible(version);
}

void parser_t::set_debug(bool debug) {
    this->debug = debug;
}

bool parser_t::get_debug() const {
    return debug;
}

bool parser_t::is_legacy_replay(const buffer_t &buffer) const {
    return buffer.size() >= 10 && buffer[8] == 0x78 && buffer[9] == 0xDA;
}

bool parser_t::is_legacy() const {
    return legacy;
}

void parser_t::decrypt_replay(buffer_t &replay_data, const unsigned char *key_data) {
    debug_stream_content("out/replay-ec.dat", replay_data.begin(), replay_data.end());
    
    BF_KEY key = {{0}};
    BF_set_key(&key, 16, key_data);
    
    const int block_size = 8;
    
    size_t padding_size = (block_size - (replay_data.size() % block_size));
    if (padding_size != 0) {
        size_t required_size = replay_data.size() + padding_size;
        replay_data.resize(required_size, 0);
    }
    
    unsigned char previous[block_size] = {0},
                  decrypted[block_size] = {0};
    for (auto it = replay_data.begin(); it != replay_data.end(); it += block_size) {
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

uint32_t parser_t::get_data_block_count(const buffer_t &buffer) const {
    // number of data blocks is contained in an 'unsigned long' (4 bytes) 
    // with an offset of 4 bytes from the beginning of te file
    const size_t db_cnt_offset = 4;
    const uint32_t *db_cnt = reinterpret_cast<const uint32_t*>(&buffer[db_cnt_offset]);
    return *db_cnt;
} 

void parser_t::extract_replay(buffer_t &compressed_replay, buffer_t &replay) {
    debug_stream_content("out/replay-c.dat", compressed_replay.begin(), compressed_replay.end());

    z_stream strm = { 
        reinterpret_cast<unsigned char*>(&(compressed_replay[0])),
        static_cast<uInt>(compressed_replay.size())
    };
    
    int ret = inflateInit(&strm);
    
    if (ret != Z_OK) {
        std::stringstream msg;
        msg << __func__ 
            << ": inflateInit() failed!";
        throw std::runtime_error(msg.str());
    }
    
    const int chunk = 10*1024*1024;
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

void parser_t::get_data_blocks(buffer_t &buffer, std::vector<slice_t> &data_blocks) const {
    if (buffer.size() == 0) {
        throw std::runtime_error("No data");
    }

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

        if (data_block_offset + *block_size > buffer.size()) {
            throw std::runtime_error("Invalid block size.");
        }

        auto data_block_end = data_block_beg + *block_size;
        data_blocks.emplace_back(data_block_beg, data_block_end);
        
        // modify offset for next data block
        data_block_sz_offset = data_block_offset + *block_size;
    }
    
    // last slice contains encrypted / compressed game replay, seperated by 8 bytes with unknown content
    auto &last_data_block = data_blocks.back();
    data_blocks.emplace_back(last_data_block.end() + 8, buffer.end());
}

void parser_t::read_packets(game_t &game) {
    packet_reader->init(game.version, &game.replay);
    while (packet_reader->has_next()) {
        game.packets.push_back(packet_reader->next());
    }
}

void parser_t::read_game_info(game_t& game) {
    // get game details
    Json::Value root;
    Json::Reader reader;
    std::string doc(game.game_begin.begin(), game.game_begin.end());
    reader.parse(doc, root);
    auto vehicles = root["vehicles"];

    auto player_name = root["playerName"].asString();

    for (auto it = vehicles.begin(); it != vehicles.end(); ++it) {
        player_t player;
        
        player.player_id = boost::lexical_cast<int>(it.key().asString());
        player.name = (*it)["name"].asString();
        player.team = (*it)["team"].asInt();
        player.tank = (*it)["vehicleType"].asString();
        player.tank = player.tank.substr(player.tank.find(':') + 1);

        if (player.name == player_name) {
            game.recorder_id = player.player_id;
        }

        game.players[player.player_id] = player;
        game.teams[player.team - 1].insert(player.player_id);
    }

    std::string map_name = root["mapName"].asString();

    if (root.isMember("gameplayType")) {
        game.game_mode = root["gameplayType"].asString();
    } else {
        // from 8.0 this is renamed
        game.game_mode = root["gameplayID"].asString();
    }

    // explicit check for game version should be better
    get_arena(map_name, game.arena, load_data_mode == load_data_mode_t::on_demand);
}

void wotreplay::show_packet_summary(const std::vector<packet_t>& packets) {
    std::map<char, int> packet_type_count;

    for (const packet_t &p : packets) {
        packet_type_count[p.type()]++;
    }

    for (auto it : packet_type_count) {
        printf("packet_type [0x%08x] = %d\n", it.first, it.second);
    }

    printf("Total packets = %lu\n", packets.size());
}

bool wotreplay::is_replayfile(const boost::filesystem::path &p) {
    return is_regular_file(p) && p.extension() == ".wotreplay" ;
}

void parser_t::load_data() {
    init_arena_definition();
    init_tank_definition();
}