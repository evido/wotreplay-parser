#include "image_util.h"
#include "image_writer.h"

#include <fstream>

using namespace wotreplay;

void image_writer_t::draw_death(const packet_t &packet, const game_t &game) {
    uint32_t killer, killed;
    std::tie(killed, killer) = packet.tank_destroyed();
    packet_t position_packet;
    bool found = game.find_property(packet.clock(), killed, property_t::position, position_packet);
    if (found) {
        draw_position(position_packet, game, this->deaths);
    }
}

void image_writer_t::draw_position(const packet_t &packet, const game_t &game, boost::multi_array<float, 3> &image) {
    uint32_t player_id = packet.player_id();

    int team_id = game.get_team_id(player_id);
    if (team_id < 0) return;

    auto shape = image.shape();
    int width = static_cast<int>(shape[2]);
    int height = static_cast<int>(shape[1]);

    float x,y;
    std::tie(x,y) = get_2d_coord( packet.position(), game, width, height);

    if (x >= 0 && y >= 0 && x <= (width - 1) && y <= (height - 1)) {
        image[team_id][y][x] = 1;

        if (player_id == game.get_recorder_id()) {
            image[2][y][x] = 1;
        }
    }
}

void image_writer_t::update(const game_t &game) {
    std::set<int> dead_players;
    for (const packet_t &packet : game.get_packets()) {
        if (packet.has_property(property_t::position)
            && dead_players.find(packet.player_id()) == dead_players.end()) {
            draw_position(packet, game, this->positions);
        } else if (packet.has_property(property_t::tank_destroyed)) {
            uint32_t target, killer;
            std::tie(target, killer) = packet.tank_destroyed();
            dead_players.insert(target);
            draw_death(packet, game);
        }
    }
}

static std::string get_map_path(const std::string &map, const std::string &mode) {
    return "maps/no-border/" + map + "_" + mode + ".png";
}

void image_writer_t::load_base_map(const std::string &map_name, const std::string &game_mode) {
    std::string mini_map = get_map_path(map_name, game_mode);
    std::ifstream is(mini_map, std::ios::binary);
    read_png(is, this->base);
}

void image_writer_t::write(std::ostream &os) {
    write_png(os, result);
}

void image_writer_t::init(const std::string &map, const std::string &mode) {
    load_base_map(map, mode);
    const size_t *shape = base.shape();
    size_t height = shape[0], width = shape[1];
    positions.resize(boost::extents[3][height][width]);
    deaths.resize(boost::extents[3][height][width]);
    initialized = true;
}

void image_writer_t::clear() {
    std::fill(base.origin(), base.origin() + base.num_elements(), 0.f);
    std::fill(deaths.origin(), deaths.origin() + deaths.num_elements(), 0.f);
    std::fill(positions.origin(), positions.origin() + positions.num_elements(), 0.f);
}

void image_writer_t::finish() {
    // copy background to result
    const size_t *shape = base.shape();
    result.resize(boost::extents[shape[0]][shape[1]][shape[2]]);
    result = base;

    for (int i = 0; i < shape[0]; ++i) {
        for (int j = 0; j < shape[1]; ++j) {
            if (positions[0][i][j] > positions[1][i][j]) {
                // position claimed by first team
                result[i][j][0] = result[i][j][2] = 0x00;
                result[i][j][1] = result[i][j][3] = 0xFF;
            } else if (positions[0][i][j] < positions[1][i][j]) {
                // position claimed by second team
                result[i][j][1] = result[i][j][2] = 0x00;
                result[i][j][0] = result[i][j][3] = 0xFF;
            } else {
                // no change
            }

            if (this->show_self && positions[2][i][j] > 0.f) {
                // position claimed by second team
                result[i][j][0] = result[i][j][1] = 0x00;
                result[i][j][2] = result[i][j][3] = 0xFF;
            }

            if (deaths[0][i][j] + deaths[1][i][j] > 0.f) {
                static int offsets[][2] = {
                    {-1, -1},
                    {-1,  0},
                    {-1,  1},
                    { 0,  1},
                    { 1,  1},
                    { 1,  0},
                    { 1, -1},
                    { 0, -1},
                    { 0,  0}
                };

                for (const auto &offset : offsets) {
                    int x = j + offset[0];
                    int y = i + offset[1];
                    result[y][x][3] = result[y][x][0] = result[y][x][1] = 0xFF;
                    result[y][x][2] = 0x00;
                }
            }
        }
    }
}

void image_writer_t::reset() {
    // initialize with empty image arrays
    result = deaths = positions = base =  boost::multi_array<uint8_t, 3>();
    initialized = false;
}

bool image_writer_t::is_initialized() const {
    return initialized;
}

void image_writer_t::set_show_self(bool show_self) {
    this->show_self = show_self;
}

bool image_writer_t::get_show_self() const {
    return show_self;
}
