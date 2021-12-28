#include "json/json.h"

#include "arena.h"
#include "cipher_context.h"
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
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <zlib.h>

using namespace wotreplay;
using namespace boost::filesystem;

#ifndef __func__
#define __func__ __FUNCTION__
#endif

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

static std::map<game_title_t, std::array<unsigned char, 16>> encryption_keys =
{
	{ game_title_t::world_of_tanks,{ 0xDE, 0x72, 0xBE, 0xA0, 0xDE, 0x04, 0xBE, 0xB1, 0xDE, 0xFE, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF } },
	{ game_title_t::world_of_warships,{ 0x29, 0xB7, 0xC9, 0x09, 0x38, 0x3F, 0x84, 0x88, 0xFA, 0x98, 0xEC, 0x4E, 0x13, 0x19, 0x79, 0xFB } }
};

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
    
    get_data_blocks(buffer, data_blocks);

    if (debug) {
        for (int i = 0; i < data_blocks.size(); ++i) {
            debug_stream_content((boost::format("data-block-%1%.dat") % i).str(),
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

    buffer_t &player_info = game.player_info;    
    player_info.resize(data_blocks[1].size());
    std::copy(data_blocks[1].begin(), data_blocks[1].end(), player_info.begin());

    if (data_blocks.size() == 3) {
        // third block contains game summary
        buffer_t &game_end = game.game_end;
        game_end.resize(data_blocks[1].size());
        std::copy(data_blocks[1].begin(), data_blocks[1].end(), game_end.begin());
    }

    raw_replay.resize(data_blocks.back().size());
    std::copy(data_blocks.back().begin(), data_blocks.back().end(), raw_replay.begin());
        
	read_player_info(game);
    read_arena_info(game);

	auto key = encryption_keys[game.get_game_title()].data();

    uint32_t decompressed_size = *reinterpret_cast<const uint32_t *>(&raw_replay[0]);
    uint32_t compressed_size = *reinterpret_cast<const uint32_t *>(&raw_replay[4]);
    decrypt_replay(raw_replay.data() + 8, raw_replay.data() + raw_replay.size(), key);

    game.replay.resize(decompressed_size, 0);
    extract_replay(raw_replay.data() + 8, raw_replay.data() + 8 + compressed_size, game.replay);

	debug_stream_content("replay.dat", game.replay.begin(), game.replay.end());
    
    // read version string
    uint32_t version_string_sz = get_field<uint32_t>(game.replay.begin(), game.replay.end(), 12);
    std::string version(game.replay.begin() + 16, game.replay.begin() + 16 + version_string_sz);
    game.version = version_t(version);
    
    if (!this->setup(game.version)) {
        logger.writef(log_level_t::warning, "Warning: Replay version (%1%) not marked as compatible.\n", game.version.text);
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

void parser_t::decrypt_replay(unsigned char *begin, unsigned char *end, const unsigned char *key_data) {
    debug_stream_content("replay-ec.dat", begin, end);
    
    const int block_size = 8;
    const int key_size = 16;
    const unsigned char iv[key_size] = {0};
    
    unsigned char previous[block_size] = {0};
    unsigned char decrypted[block_size] = {0};

    CipherContext cipherContext("BF-ECB", key_data, key_size, iv);

    uint32_t pin = 0;
    uint32_t pout = 0;
    int decrypted_len;
    while (pin < (end - begin)) {
        cipherContext.update(decrypted, &decrypted_len, begin + pin, block_size);

        std::transform(previous, previous + decrypted_len, decrypted, decrypted, std::bit_xor<unsigned char>());
        std::copy_n(decrypted, block_size, previous);
        std::copy_n(decrypted, block_size, begin + pout);

        pin += block_size;
        pout += decrypted_len;
    }

    cipherContext.finalize(decrypted, &decrypted_len);
    std::transform(previous, previous + decrypted_len, decrypted, decrypted, std::bit_xor<unsigned char>());
    std::copy_n(decrypted, block_size, begin + pout);
}

uint32_t parser_t::get_data_block_count(const buffer_t &buffer) const {
    // number of data blocks is contained in an 'unsigned long' (4 bytes) 
    // with an offset of 4 bytes from the beginning of te file
    const size_t db_cnt_offset = 4;
    const uint32_t *db_cnt = reinterpret_cast<const uint32_t*>(&buffer[db_cnt_offset]);
    return *db_cnt;
} 

void parser_t::extract_replay(const unsigned char* begin, const unsigned char* end, buffer_t &replay) {
    debug_stream_content("replay-c.dat", begin, end);

    z_stream strm = { 
        const_cast<unsigned char*>(begin),
        static_cast<uInt>(end - begin)
    };
    
    int ret = inflateInit(&strm);
    
    if (ret != Z_OK) {
        std::stringstream msg;
        msg << __func__ 
            << ": inflateInit() failed!";
        throw std::runtime_error(msg.str());
    }
    
    const int chunk = 1024 * 1024;
    int p = 0;
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
        std::copy_n(out.get(), have, &replay[p]);
        p += have;
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
    // uint32_t decompressed_size = *reinterpret_cast<const uint32_t *>(&buffer[data_block_sz_offset]);
    uint32_t compressed_size = *reinterpret_cast<const uint32_t *>(&buffer[data_block_sz_offset + 4]);

    auto start = data_block_sz_offset;
    auto end = start + 8 + ((compressed_size + 7) / 8)*8;
    data_blocks.emplace_back(buffer.begin() + start, buffer.begin() + end);
}

void parser_t::read_packets(game_t &game) {
    packet_reader->init(game.version, &game.replay, game.title);
    while (packet_reader->has_next()) {
        game.packets.reserve(500000);
        game.packets.push_back(packet_reader->next());
    }

#ifndef _DEBUG
    game.packets.shrink_to_fit();
#endif
}

void parser_t::read_arena_info(game_t& game) {
    // get game details
    Json::Value root;
    Json::Reader reader;
    std::string doc(game.game_begin.begin(), game.game_begin.end());
    reader.parse(doc, root);

    if (root.isMember("gameplayType")) {
        game.game_mode = root["gameplayType"].asString();
    } else if (root.isMember("gameplayID")) {
        // from 8.0 this is renamed
        game.game_mode = root["gameplayID"].asString();
    }

	std::string map_name = root["mapName"].asString();

	// explicit check for game version should be better
	get_arena(map_name, game.arena, load_data_mode == load_data_mode_t::on_demand);
}

void parser_t::read_player_info(game_t& game) {
    // get game details
    Json::Value root;
    Json::Reader reader;
    std::string doc(game.player_info.begin(), game.player_info.end());
    reader.parse(doc, root);

	auto player_account_id = root[0]["personal"]["avatar"]["accountDBID"].asString();
    auto player_name = root[0]["players"][player_account_id]["name"].asString();

    auto vehicles = root[1];

	// if (vehicles.isArray()) {
	// 	// world of warships
	// 	for (auto it = vehicles.begin(); it != vehicles.end(); ++it) {
	// 		player_t player;

	// 		player.player_id = (*it)["id"].asUInt();
	// 		player.team = (*it)["relation"].asInt();
	// 		player.tank = (*it)["shipId"].asString();

	// 		if (player.name == player_name) {
	// 			game.recorder_id = player.player_id;
	// 		}

	// 		if (player.player_id != 0) { // relation of player is 0
	// 			game.players[player.player_id] = player;
	// 		}			
	// 	}

	// 	game.game_mode = root["scenarioConfigId"].asString();
	// 	game.title = game_title_t::world_of_warships;
	// }

    // world of tanks
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

    game.title = game_title_t::world_of_tanks;
}

void wotreplay::show_packet_summary(const std::vector<packet_t>& packets) {
    std::map<char, int> packet_type_count;

    for (const packet_t &p : packets) {
        packet_type_count[p.type()]++;
    }

    for (const auto &it : packet_type_count) {
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
