#include "json_writer.h"

using namespace wotreplay;

void json_writer_t::init(const arena_t &arena, const std::string &mode) {
    root = (Json::Value(Json::objectValue));
    Json::Value mapValue(arena.name.c_str());
    Json::Value modeValue(mode.c_str());

    root["map"] = mapValue;
    root["mode"] = modeValue;
    root["packets"] = Json::Value(Json::arrayValue);
    root["map_boundaries"] = Json::Value(Json::arrayValue);
    
    this->initialized = true;
}

void json_writer_t::write(std::ostream &os) {
    Json::FastWriter writer;
    os << writer.write(root);
}

json_writer_t::json_writer_t()
    : filter([](const packet_t &){ return true; })
{}

void json_writer_t::update(const game_t &game) {
    auto &packets = root["packets"];

    bounding_box_t bounding_box = game.get_arena().bounding_box;

    // copy boundary values
    Json::Value coordinate(Json::arrayValue);
    coordinate.append(std::get<0>(bounding_box.bottom_left));
    coordinate.append(std::get<1>(bounding_box.bottom_left));
    root["map_boundaries"].append(coordinate);
    coordinate.clear();
    coordinate.append(std::get<0>(bounding_box.upper_right));
    coordinate.append(std::get<1>(bounding_box.upper_right));
    root["map_boundaries"].append(coordinate);
    
    root["recorder_id"] = game.get_recorder_id();

    Json::Value summary;
    Json::Reader reader;
    
    const buffer_t &buffer = game.get_game_begin();
    std::string summary_str(buffer.begin(), buffer.end());
    reader.parse(summary_str, summary);
    
    root["summary"] = summary;

    const version_t &v = game.get_version();

    for (const auto &packet : game.get_packets()) {
        // skip empty packet
        if (!filter(packet)) continue;

        auto &value = packets.append(Json::objectValue);

        if (packet.has_property(property_t::type)) {
            value["type"] = packet.type();
        }
        
        if (packet.has_property(property_t::clock)) {
            value["clock"] = packet.clock();
        }

        if (packet.has_property(property_t::player_id)) {
            value["player_id"] = packet.player_id();
            int team_id = game.get_team_id(packet.player_id());
            if (team_id != -1) {
                value["team"] = team_id;
            }
        }

        if (packet.has_property(property_t::position)) {
            auto &positionValue = value["position"] = Json::Value(Json::arrayValue);
            const auto &position = packet.position();
            positionValue.append(std::get<0>(position));
            positionValue.append(std::get<1>(position));
            positionValue.append(std::get<2>(position));
        }

        if (packet.has_property(property_t::tank_destroyed)) {
            uint32_t target, destroyed_by;
            std::tie(target, destroyed_by) = packet.tank_destroyed();
            value["target"] = target;
            value["destroyed_by"] = destroyed_by;
        }

        // disable untill new locations can be found
        if ((v.major == 9 && v.minor < 10) || v.major < 9) {
            if (packet.has_property(property_t::health)) {
                value["health"] = packet.health();
            }

            if (packet.has_property(property_t::source)) {
                value["source"] = packet.source();
            }
        }

        if (packet.has_property(property_t::message)) {
            value["message"] = packet.message();
        }

        if (packet.has_property(property_t::sub_type)) {
            value["sub_type"] = packet.sub_type();
        }

        if (packet.has_property(property_t::target)) {
            value["target"] = packet.target();
        }

        if (packet.has_property(property_t::destroyed_track_id)) {
            value["destroyed_track_id"] = packet.destroyed_track_id();
        }

        if (packet.has_property(property_t::alt_track_state)) {
            value["alt_track_state"] = packet.alt_track_state();
        }
    }
}

void json_writer_t::finish() {
    // empty
}

void json_writer_t::reset() {
    root.clear();
    this->initialized = false;
}

bool json_writer_t::is_initialized() const {
    return initialized;
}

void json_writer_t::clear() {
    root["packets"].clear();
}

void json_writer_t::set_filter(filter_t filter) {
    this->filter = filter;
}