#include "json_writer.h"

using namespace wotreplay;

void json_writer_t::init(const std::string &map, const std::string &mode) {
    value = Json::Value(Json::objectValue);

    Json::Value mapValue(map.c_str());
    Json::Value modeValue(mode.c_str());

    value["map"] = mapValue;
    value["mode"] = modeValue;
    value["packets"] = Json::Value(Json::arrayValue);

    this->initialized = true;
}

void json_writer_t::write(std::ostream &os) {
    os << value;
}

void json_writer_t::update(const game_t &game) {
    auto &packets = value["packets"];

    value["recorder_id"] = game.get_recorder_id();

    for (auto &packet : game.get_packets()) {
        Json::Value value(Json::objectValue);

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
            Json::Value positionValue(Json::arrayValue);
            const auto &position = packet.position();
            positionValue.append(std::get<0>(position));
            positionValue.append(std::get<1>(position));
            positionValue.append(std::get<2>(position));
            value["position"] = positionValue;
        }

        if (packet.has_property(property_t::tank_destroyed)) {
            uint32_t target, destroyed_by;
            std::tie(target, destroyed_by) = packet.tank_destroyed();
            value["target"] = target;
            value["destroyed_by"] = destroyed_by;
        }

        if (packet.has_property(property_t::health)) {
            value["health"] = packet.health();
        }

        packets.append(value);
    }
}

void json_writer_t::finish() {
    // empty
}

void json_writer_t::reset() {
    value.clear();
    this->initialized = false;
}

bool json_writer_t::is_initialized() const {
    return initialized;
}

void json_writer_t::clear() {
    value["packets"].clear();
}