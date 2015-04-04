#include "class_heatmap_writer.h"
#include "image_util.h"
#include "logger.h"

#include <boost/algorithm/clamp.hpp>

using namespace wotreplay;
using boost::algorithm::clamp;

void class_heatmap_writer_t::set_draw_rules(const std::vector<draw_rule_t> &rules) {
    this->rules = rules;
}

const std::vector<draw_rule_t> &class_heatmap_writer_t::get_draw_rules() const {
    return this->rules;
}

void class_heatmap_writer_t::init(const wotreplay::arena_t &arena, const std::string &mode) {
    this->arena = arena;
    this->game_mode = mode;

    classes.clear();

    int class_count = 0;
    for (const auto &rule : rules) {
        if (classes.find(rule.color) == classes.end()) {
            classes[rule.color] = class_count;
            class_count += 1;
        }
    }

    positions.resize(boost::extents[class_count][image_height][image_width]);

    clear();

    initialized = true;
}

void class_heatmap_writer_t::update(const wotreplay::game_t &game) {
    std::set<int> dead_players;
    virtual_machine_t vm(game, rules);

    const auto &packets = game.get_packets();
    int i = 0, offset = get_start_packet(game, skip);
    for (auto it = packets.begin(); it != packets.end() ; it++ ) {
        const auto &packet = *it;

        i += 1;

        if (!packet.has_property(property_t::position)) {
            if (packet.has_property(property_t::tank_destroyed)) {
                uint32_t target, killer;
                std::tie(target, killer) = packet.tank_destroyed();
                dead_players.insert(target);
            }
            continue;
        }

        if (i < offset) {
            continue;
        }

        uint32_t player_id = packet.player_id();

        int rule_id = vm(packet);
        if (rule_id < 0 || dead_players.find(player_id) != dead_players.end()) {
            continue;
        }

        int class_id = classes[rules[rule_id].color];

        const bounding_box_t &bounding_box = game.get_arena().bounding_box;
        std::tuple<float, float> position = get_2d_coord(packet.position(), bounding_box, image_width, image_height);
        double x = std::get<0>(position);
        double y = std::get<1>(position);

        if (x >= 0 && y >= 0 && x <= (image_width - 1) && y <= (image_height - 1)) {
            float px = x - floor(x), py = y - floor(y);
            positions[class_id][floor(y)][floor(x)] += px*py;
            positions[class_id][ceil(y)][floor(x)] += px*(1-py);
            positions[class_id][floor(y)][ceil(x)] += (1-px)*py;
            positions[class_id][ceil(y)][ceil(x)] += (1-px)*(1-py);
        }
    }
}

void class_heatmap_writer_t::finish() {
    draw_basemap();

    const size_t *shape = base.shape();
    result.resize(boost::extents[shape[0]][shape[1]][shape[2]]);
    result = base;

    int class_count = classes.size();

    uint32_t colors[class_count];
    for (const auto &it : classes) {
        colors[it.second] = it.first;
    }

    double min[class_count], max[class_count];

    for (int k = 0; k < class_count; k += 1) {
        std::tie(min[k], max[k]) = get_bounds(positions[k],
                                              std::get<0>(bounds),
                                              std::get<1>(bounds));
    }

    for (int i = 0; i < image_width; i += 1) {
        for (int j = 0; j < image_height; j += 1) {
            int ix = 0;
            double a = clamp((positions[0][i][j] - min[0]) / (max[0] - min[0]), 0.0, 1.0);
            for (int k = 1; k < class_count; k += 1) {
                double _a = clamp((positions[k][i][j] - min[k]) / (max[k] - min[k]), 0.0, 1.0);
                if (_a > a) {
                    a = _a;
                    ix = k;
                }
            }

            uint32_t c = colors[ix];
            if (!no_basemap) {
                a *= (c & 0xFF) / 255.;
                result[i][j][0] = mix(result[i][j][0], result[i][j][0], 1. - a, (c >> 24) & 0xFF, a);
                result[i][j][1] = mix(result[i][j][1], result[i][j][1], 1. - a, (c >> 16) & 0xFF, a);
                result[i][j][2] = mix(result[i][j][2], result[i][j][2], 1. - a, (c >>  8) & 0xFF, a);
            } else {
                result[i][j][0] = (c >> 24) & 0xFF;
                result[i][j][1] = (c >> 16) & 0xFF;
                result[i][j][2] = (c >>  8) & 0xFF;
                result[i][j][3] = (c & 0xFF)* a;
            }
        }
    }
}