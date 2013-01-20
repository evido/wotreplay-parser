#include "json/json.h"
#include "parser.h"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <map>
#include <memory>
#include <openssl/blowfish.h>
#include <ostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <zlib.h>

// use libcpp regex implementation if possible
#ifdef _LIBCPP_VERSION

using std::regex;
using std::smatch;
using std::regex_search;

#else

// fallback to boost
#include <boost/regex.hpp>
using boost::regex;
using boost::smatch;
using boost::regex_search;

#endif

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

parser_t::parser_t(bool debug)
    : debug(debug)
{
    // empty
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
    game.version.assign(game.replay.begin() + 16, game.replay.begin() + 16 + version_string_sz);

    if (!this->is_compatible(game)) {
        std::cerr << boost::format("Warning: Replay version (%1%) not marked as compatible.\n") % game.version;
    }
    
    read_packets(game);

    if (debug) {
        show_packet_summary(game.get_packets());
    }
}

bool parser_t::is_compatible(const game_t &game) const {
    std::string version;
    regex re(R"(v\.(\d+\.\d+\.\d+))");
    smatch match;

    if (regex_search(game.version, match, re)) {
        version = match[1];
    }

    std::vector<std::string> compatible_versions{
        "0.8.2"
    };

    return std::find(compatible_versions.begin(), compatible_versions.end(), version)
                != compatible_versions.end();
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

void parser_t::get_data_blocks(buffer_t &buffer, std::vector<slice_t> &data_blocks) const {
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
    {0x1F, 17},
    {0x20, 21},  // modified for 0.8.0 {0x20, 4}
    {0x31,  4},  // indication of restart of the replay, probably not a part of the replay but necessary because of wrong detection of the start of the replay
    {0x0D, 22},
    {0x00, 22}
};

size_t parser_t::read_packets(game_t &game) {
    buffer_t &buffer = game.replay;

    if (is_legacy()) {
        packet_lengths[0x16] = 44;
    }

    size_t offset = 0;

    std::array<uint8_t, 6> marker = {{0x2C, 0x01, 0x01, 0x00, 0x00, 0x00}};

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
            size_t count = game.packets.size();
            if (count < 500) {
                ix = static_cast<int>(++offset);
                if (debug) {
                    std::cerr << ix << "\n";
                    std::cerr << "WARNING INCORRECT OFFSET!\n";
                }
                continue;
            } else {
                int unread = (int) buffer.size() - (int) ix;
                if (unread > 25) {
                    std::cerr <<
                        boost::format("Warning: Unexpected end of replay, %1d bytes left.\n") % unread;
                }
                if (debug) {
                    const packet_t &last_packet = game.packets.back();
                    const slice_t &packet_data = last_packet.get_data();
                    size_t packet_size = packet_data.size();
                    size_t prev_ix = ix - packet_size;
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
            case 0x1F:
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
            game.packets.emplace_back(parser_t::read_packet(packet_begin, packet_end));
        } else if (debug) {
            std::cerr << "Packet went out of replay file bounds.\n";
        }

        if (debug) {
            std::cerr << boost::format("[%2%] type=0x%1$02X size=%3%\n") % (int) buffer[ix + 1] % ix % packet_length;
        }
        
        ix += packet_length;
    }

    return ix;
}

static std::map<std::string, std::array<int, 4>> map_boundaries =
{
    { "01_karelia",         {{ -500, 500, -500, 500 }} },
    { "02_malinovka",       {{ -500, 500, -500, 500 }} },
    { "03_campania",        {{ -300, 300, -300, 300 }} },
    { "04_himmelsdorf",     {{ -300, 400, -300, 400 }} },
    { "05_prohorovka", 	    {{ -500, 500, -500, 500 }} },
    { "06_ensk",            {{ -300, 300, -300, 300 }} },
    { "07_lakeville",       {{ -400, 400, -400, 400 }} },
    { "08_ruinberg",        {{ -400, 400, -400, 400 }} },
    { "10_hills",           {{ -400, 400, -400, 400 }} },
    { "11_murovanka",       {{ -400, 400, -400, 400 }} },
    { "13_erlenberg",       {{ -500, 500, -500, 500 }} },
    { "14_siegfried_line",  {{ -500, 500, -500, 500 }} },
    { "15_komarin",         {{ -400, 400, -400, 400 }} },
    { "17_munchen",         {{ -300, 300, -300, 300 }} },
    { "18_cliff",           {{ -500, 500, -500, 500 }} },
    { "19_monastery",       {{ -500, 500, -500, 500 }} },
    { "22_slough",          {{ -500, 500, -500, 500 }} },
    { "23_westfeld",        {{ -500, 500, -500, 500 }} },
    { "28_desert",          {{ -500, 500, -500, 500 }} },
    { "29_el_hallouf",      {{ -500, 500, -500, 500 }} },
    { "31_airfield",        {{ -500, 500, -500, 500 }} },
    { "33_fjord",           {{ -500, 500, -500, 500 }} },
    { "34_redshire",        {{ -500, 500, -500, 500 }} },
    { "35_steppes",         {{ -500, 500, -500, 500 }} },
    { "36_fishing_bay",     {{ -500, 500, -500, 500 }} },
    { "37_caucasus",        {{ -500, 500, -500, 500 }} },
    { "38_mannerheim_line", {{ -500, 500, -500, 500 }} },
    { "39_crimea",          {{ -500, 500, -500, 500 }} },
    { "42_north_america",   {{ -400, 400, -400, 400 }} },
    { "44_north_america",   {{ -500, 500, -500, 500 }} },
    { "45_north_america",   {{ -500, 500, -500, 500 }} },
    { "47_canada_a",        {{ -500, 500, -500, 500 }} },
    { "51_asia",            {{ -500, 500, -500, 500 }} }
};

void parser_t::read_game_info(game_t& game) {
    // get game details
    Json::Value root;
    Json::Reader reader;
    std::string doc(game.game_begin.begin(), game.game_begin.end());
    reader.parse(doc, root);
    auto vehicles = root["vehicles"];

    auto player_name = root["playerName"].asString();


    for (auto it = vehicles.begin(); it != vehicles.end(); ++it) {
        unsigned player_id = boost::lexical_cast<int>(it.key().asString());
        std::string name = (*it)["name"].asString();
        if (name == player_name) {
            game.recorder_id = player_id;
        }
        int team_id = (*it)["team"].asInt();
        game.teams[team_id - 1].insert(player_id);
    }

    game.map_name = root["mapName"].asString();

    if (root.isMember("gameplayType")) {
        game.game_mode = root["gameplayType"].asString();
    } else {
        // from 8.0 this is renamed
        game.game_mode = root["gameplayID"].asString();
    }
    
    game.game_mode.resize(3);

    // explicit check for game version should be better
    if (map_boundaries.find(game.map_name) == map_boundaries.end()) {

        if (game.map_name == "north_america") {
            game.map_name = "44_north_america";
        } else {
            for (auto entry : map_boundaries) {
                std::string map_name = entry.first;
                std::string short_map_name(map_name.begin() + 3, map_name.end());
                if (short_map_name == game.map_name) {
                    // rewrite map name
                    game.map_name = map_name;
                }
            }
        }
    }

    game.map_boundaries = map_boundaries[game.map_name];
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

bool wotreplay::is_replayfile(const boost::filesystem::path &p) {
    return is_regular_file(p) && p.extension() == ".wotreplay" ;
}

static size_t get_unread_bytes(const game_t &game) {
    const std::vector<packet_t> &packets = game.get_packets();
    const packet_t &last_packet = packets.back();
    const slice_t &data = last_packet.get_data();
    const buffer_t &replay = game.get_raw_replay();
    size_t bytes_unread = &replay.back() - &data.back();
    return bytes_unread;
}

static size_t get_dead_player_count(const buffer_t &game_end) {
    // parse game_end formatted as a JSON object
    Json::Value root;
    Json::Reader reader;
    std::string doc(game_end.begin(), game_end.end());
    reader.parse(doc, root);
    // TODO: validate this node
    Json::Value players = root[1];
    size_t dead_players = 0;
    for (auto it = players.begin(); it != players.end(); ++it) {
        bool isAlive = (*it)["isAlive"].asBool();
        if (!isAlive) ++dead_players;
    }
    
    return dead_players;
}

void wotreplay::validate_parser(const std::string &path) {
    // loop over each file in the path
    for (directory_iterator it(path); it != directory_iterator(); ++it) {
        // skip non-replay files
        if (!is_replayfile(it->path())) continue;

        std::stringstream ss;

        ss << "file:" << it->path().string();
        
        // parse the path as a replay file, using methods internal to parser
        std::ifstream is(it->path().string(), std::ios::binary);
        parser_t parser(false);
        game_t game;
        parser.parse(is, game);
        is.close();

        size_t bytes_unread = get_unread_bytes(game);

        ss << " end:" << (bytes_unread <= 25) << " ";

        const buffer_t &game_end = game.get_game_end();

        // game_end can be unavailable when the replay is incomplete
        if (game_end.size() > 0)  {
            const std::vector<packet_t> &packets = game.get_packets();
            size_t tank_destroyed_count = std::count_if(packets.begin(), packets.end(), [](const packet_t &packet) {
                return packet.has_property(property_t::tank_destroyed);
            });

            size_t dead_players = get_dead_player_count(game_end);
            ss << boost::format("kills: %d") % (tank_destroyed_count == dead_players);
        } else {
            ss << "n/a";
        }

        std::cout << ss.str() << "\n";
    }
}
