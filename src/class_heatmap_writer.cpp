#include "class_heatmap_writer.h"
#include "image_util.h"
#include "logger.h"

#include <boost/algorithm/clamp.hpp>

using namespace wotreplay;
using boost::algorithm::clamp;

void class_heatmap_writer_t::set_draw_rules(const std::vector<draw_rule_t> &rules)
{
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

int class_heatmap_writer_t::get_class(const game_t &game, const packet_t &packet) const {
    virtual_machine_t vm(game, rules);
    int rule_id = vm(packet);
    return rule_id < 0 ? -1 : classes.at(rules[rule_id].color);
}

void class_heatmap_writer_t::finish() {
    draw_basemap();

    const size_t *shape = base.shape();
    result.resize(boost::extents[shape[0]][shape[1]][shape[2]]);
    result = base;

    const int class_count = classes.size();

	std::unique_ptr<uint32_t[]> colors(new uint32_t[class_count]);
    for (const auto &it : classes) {
        colors[it.second] = it.first;
    }

	std::unique_ptr<double[]> min(new double[class_count]);
	std::unique_ptr<double[]> max(new double[class_count]);


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