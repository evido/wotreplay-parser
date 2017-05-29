#include "game.h"
#include "regex.h"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_map>

using namespace wotreplay;


const std::vector<packet_t> &game_t::get_packets() const {
    return packets;
}

const std::string &game_t::get_map_name() const {
    return arena.name;
}

const std::string &game_t::get_game_mode() const {
    return game_mode;
}

const arena_t &game_t::get_arena() const {
    return arena;
}

const std::set<int> &game_t::get_team(int team_id) const {
    return teams[team_id];
}

uint32_t game_t::get_recorder_id() const {
    return recorder_id;
}

bool game_t::find_property(uint32_t clock, uint32_t player_id, property_t property, packet_t &out) const {
    // inline function function for using with stl to finding the range with the same clock
    auto has_same_clock = [&](const packet_t &target) -> bool  {
        // packets without clock are included
        return target.has_property(property_t::clock)
            && target.clock() == clock;
    };

    // find first packet with same clock
    auto it_clock_begin = std::find_if(packets.cbegin(), packets.cend(), has_same_clock);
    // find last packet with same clock
    auto it_clock_end = std::find_if_not(it_clock_begin, packets.cend(), has_same_clock);

    auto is_related_with_property = [&](const packet_t &target) -> bool {
        return target.has_property(property_t::clock) &&
            target.has_property(property_t::player_id) &&
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

int game_t::get_team_id(int player_id) const {
    auto it = std::find_if(teams.begin(), teams.end(), [&](const std::set<int> &team) {
        return team.find(player_id) != team.end();
    });
    return it == teams.end() ? -1 : (static_cast<int>(it - teams.begin()));
}

const version_t &game_t::get_version() const {
    return version;
}

const buffer_t &game_t::get_game_begin() const {
    return game_begin;
}

const buffer_t &game_t::get_game_end() const {
    return game_end;
}

const buffer_t &game_t::get_raw_replay() const {
    return replay;
}

void wotreplay::write_parts_to_file(const game_t &game) {
    std::ofstream game_begin("game_begin.json", std::ios::binary | std::ios::ate);
    std::copy(game.get_game_begin().begin(),
              game.get_game_begin().end(),
              std::ostream_iterator<char>(game_begin));
    game_begin.close();

    std::ofstream game_end("game_end.json", std::ios::binary | std::ios::ate);
    std::copy(game.get_game_end().begin(),
              game.get_game_end().end(),
              std::ostream_iterator<char>(game_end));
    game_end.close();

    std::ofstream replay_content("replay.dat", std::ios::binary | std::ios::ate);
    std::copy(game.get_raw_replay().begin(),
              game.get_raw_replay().end(),
              std::ostream_iterator<char>(replay_content));
}

std::tuple<float, float> wotreplay::get_2d_coord(const std::tuple<float, float, float> &position, const bounding_box_t &bounding_box, int width, int height)
{
    float min_x, max_x, min_y, max_y;
    std::tie(min_x, min_y) = bounding_box.bottom_left;
    std::tie(max_x, max_y) = bounding_box.upper_right;
    // std::tie(x,z,y) = position;
    float x = std::get<0>(position);
    float y = std::get<2>(position);
    x = (x - min_x) * (width - 1) / (max_x - min_x + 1)  ;
    y = (max_y - y) * (height - 1) / (max_y - min_y + 1) ;
    return std::make_tuple(x,y);
}

void wotreplay::show_map_boundaries(const game_t &game, const std::vector<packet_t> &packets) {
    float min_x = 0.f,
        min_y = 0.f,
        max_x = 0.f,
        max_y = 0.f;

    for (const packet_t &packet : packets) {
        if (packet.type() != 0xa) {
            continue;
        }

        uint32_t player_id = packet.player_id();
        // only take into account positions of 'valid' players.
        if (game.get_team_id(player_id) != -1) {
            // update the boundaries
            auto position = packet.position();
            min_x = std::min(min_x, std::get<0>(position));
            min_y = std::min(min_y, std::get<2>(position));
            max_x = std::max(max_x, std::get<0>(position));
            max_y = std::max(max_y, std::get<2>(position));
            break;
        }
    }

    printf("The boundaries of used positions in this replay are: min_x = %f max_x = %f min_y = %f max_y = %f\n", min_x, max_x, min_y, max_y);
}

version_t::version_t(const std::string & text)
    : text(text)
{
    regex re(R"(v\.(\d+)\.(\d+)\.(\d+))");
    smatch match;
    if (regex_search(text, match, re)) {
        major = boost::lexical_cast<int>(match[2]);
        minor = boost::lexical_cast<int>(match[3]);
    } else {
        regex re(R"((\d+)[\.,] *(\d+)[\.,] *(\d+)[\.,]? *(\d+))");
        if (regex_search(text, match, re)) {
            major = boost::lexical_cast<int>(match[2]);
            minor = boost::lexical_cast<int>(match[3]);
        } else {
            major = 0;
            minor = 0;
        }
    }
}

double wotreplay::dist(const std::tuple<float, float, float> &begin,
                   const std::tuple<float, float, float> &end) {
    float dy = std::get<2>(begin) - std::get<2>(end),
    dx = std::get<0>(begin) - std::get<0>(end),
    dz = std::get<1>(begin) - std::get<1>(end);

    return std::sqrt(dy * dy + dx * dx);
}

int wotreplay::get_start_packet (const game_t &game, double skip) {
    std::unordered_map<int, packet_t> last_packets;
    int i = 0;

    for (const auto &packet : game.get_packets()) {
        i++;

        if (!packet.has_property(property_t::position)) {
            continue;
        }

        int player_id = packet.player_id(),
        team_id = game.get_team_id(player_id);


        if (team_id < 0) {
            continue; // belongs to no team
        }

        auto last = last_packets.find(player_id);
        if (last != last_packets.end()) {
            int distance = dist(last->second.position(), packet.position());
            if (distance > 0.01) {
                break;
            }
        }

        last_packets[player_id] = packet;
    }

    const auto &packets = game.get_packets();

    if (i < packets.size()) {
        float clock = packets[i].clock(),
        offset = skip;
        for (auto it = packets.begin() + i; it != packets.end(); ++it) {
            if (it->has_property(property_t::clock) &&
                it->clock() >= clock + offset) {
                break;
            }
            ++i;
        }
    }
    
    return i;
}

const player_t &game_t::get_player(int player) const {
    return players.at(player);
}

game_title_t game_t::get_game_title() const {
	return title;
}