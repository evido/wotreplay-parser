#include "packet.h"

#include <boost/format.hpp>
#include <sstream>

using namespace wotreplay;

packet_t::packet_t(const slice_t &data) {
    this->set_data(data);
}

uint32_t packet_t::type() const {
    assert(has_property(property_t::type));
    return get_field<uint32_t>(data.begin(), data.end(), 4);
}

uint32_t packet_t::player_id() const {
    assert(has_property(property_t::player_id));
    return get_field<uint32_t>(data.begin(), data.end(), 12);
}

float packet_t::clock() const {
    assert(has_property(property_t::clock));
    return get_field<float>(data.begin(), data.end(), 8);
}

std::tuple<float, float, float> packet_t::position() const {
    assert(type() == 0x0A);
    float x = get_field<float>(data.begin(), data.end(), 20);
    float y = get_field<float>(data.begin(), data.end(), 24);
    float z = get_field<float>(data.begin(), data.end(), 28);
    return std::make_tuple(x,y,z);
}

std::tuple<float, float, float> packet_t::hull_orientation() const {
    assert(has_property(property_t::hull_orientation));
    float x = get_field<float>(data.begin(), data.end(), 48);
    float y = get_field<float>(data.begin(), data.end(), 52);
    float z = get_field<float>(data.begin(), data.end(), 56);
    return std::make_tuple(x,y,z);
}

float packet_t::turret_orientation() const {
    assert(property_t::turret_orientation);
    return get_field<float>(data.begin(), data.end(), 56);
}

uint16_t packet_t::health() const {
    assert(has_property(property_t::health));
    return get_field<uint16_t>(data.begin(), data.end(), 24);
}

const std::array<bool, static_cast<size_t>(property_t::property_nr_items)> &packet_t::get_properties() const {
    return properties;
}

bool packet_t::has_property(property_t p) const {
    return properties[static_cast<size_t>(p)];
}

void packet_t::set_data(const slice_t &data) {
    this->data = data;

    // reset all properties
    std::fill(properties.begin(), properties.end(), false);

    // enable default properties
    properties[static_cast<size_t>(property_t::type)] = true;
    properties[static_cast<size_t>(property_t::length)] = true;
    
    switch(get_field<uint32_t>(data.begin(), data.end(), 4)) {
        case 0x03:
        case 0x05:
            properties[static_cast<size_t>(property_t::clock)] = true;
            properties[static_cast<size_t>(property_t::player_id)] = true;
            break;
        case 0x0a:
            properties[static_cast<size_t>(property_t::position)] = true;
            properties[static_cast<size_t>(property_t::hull_orientation)] = true;
            properties[static_cast<size_t>(property_t::clock)] = true;
            properties[static_cast<size_t>(property_t::player_id)] = true;
            break;
        case 0x07: {
            properties[static_cast<size_t>(property_t::clock)] = true;
            properties[static_cast<size_t>(property_t::player_id)] = true;
            properties[static_cast<size_t>(property_t::sub_type)] = true;
            properties[static_cast<size_t>(property_t::health)] = sub_type() == 0x05;
            properties[static_cast<size_t>(property_t::destroyed_track_id)] = sub_type() == 0x07;
            break;
        }
        case 0x08: {
            if (data.size() >= 28) {
                auto signature = get_field<uint32_t>(data.begin(), data.end(), 24);
                properties[static_cast<size_t>(property_t::tank_destroyed)] = 0x02801306 == signature;
            }
            properties[static_cast<size_t>(property_t::clock)] = true;
            properties[static_cast<size_t>(property_t::player_id)] = true;
            properties[static_cast<size_t>(property_t::sub_type)] = true;
            switch (this->sub_type()) {
                case 0x01:
                    properties[static_cast<size_t>(property_t::source)] = true;
                    properties[static_cast<size_t>(property_t::health)] = true;
                case 0x02:
                    // < 8.5
                    // properties[static_cast<size_t>(property_t::health)] = true;
                    // properties[static_cast<size_t>(property_t::source)] = true;
                    break;
                case 0x05:
                    // hit
                    properties[static_cast<size_t>(property_t::source)] = true;
                    break;
                case 0x0B:
                    // module damage
                    properties[static_cast<size_t>(property_t::source)] = true;
                    properties[static_cast<size_t>(property_t::target)] = true;
                    break;
                case 0x11:
                    // tracer information
                    break;
                case 0x17:
                    // tracked ?
                    properties[static_cast<size_t>(property_t::target)] = true;
                    break;
                case 0x19:
                    // related to tank destroyed
                    break;
                case 0x1d:
                    break;
            }
            break;
        }
        case 0x23: {
            properties[static_cast<size_t>(property_t::clock)] = true;
            properties[static_cast<size_t>(property_t::message)] = true;
            break;
        }
        case 0x20: {
            properties[static_cast<size_t>(property_t::clock)] = true;
            properties[static_cast<size_t>(property_t::player_id)] = true;
            break;
        }
        default: {
            properties[static_cast<size_t>(property_t::clock)] = data.size() >= 13;
            break;
        }
    }
}

uint32_t packet_t::sub_type() const {
    assert(has_property(property_t::sub_type));
    return get_field<uint32_t>(data.begin(), data.end(), 16);
}

uint8_t packet_t::destroyed_track_id() const {
    assert(has_property(property_t::destroyed_track_id));
    uint8_t destroyed_track_id = 0;
    switch(type()) {
        case 0x07:
            if (get_field<uint32_t>(data.begin(), data.end(), 20) == 5) {
                destroyed_track_id = get_field<uint8_t>(data.begin(), data.end(), 28);
            }
            break;
        case 0x20:
            destroyed_track_id = get_field<uint8_t>(data.begin(), data.end(), 22);
            break;
    }
    return destroyed_track_id;
}

uint8_t packet_t::alt_track_state() const {
    assert(has_property(property_t::alt_track_state));
    return get_field<uint8_t>(data.begin(), data.end(), 21);
}

uint32_t packet_t::source() const {
    assert(has_property(property_t::source));
    int pos;
    switch (sub_type()) {
        case 0x01:
            pos = 26;
            break;
        case 0x0B:
            pos = 30;
            break;
        default:
            pos = 24;
            break;
    }
    return get_field<uint32_t>(data.begin(), data.end(), pos);
}

uint32_t packet_t::target() const {
    assert(has_property(property_t::target));
    int pos = (sub_type() == 0x17) ? 28 : 24;
    return get_field<uint32_t>(data.begin(), data.end(), pos);
}

const slice_t &packet_t::get_data() const {
    return data;
}

std::tuple<uint32_t, uint32_t, uint8_t> packet_t::tank_destroyed() const {
    assert(has_property(property_t::tank_destroyed));
    return std::make_tuple(
        get_field<uint32_t>(data.begin(), data.end(), 30),
        get_field<uint32_t>(data.begin(), data.end(), 35),
        get_field<uint8_t>(data.begin(), data.end(), 42)
    );
}

std::string packet_t::message() const {
    size_t field_size = get_field<uint32_t>(data.begin(), data.end(), 12);
    return std::string(data.begin() + 16, data.begin() + 16 + field_size);
}

uint32_t packet_t::length() const {
    assert(has_property(property_t::length));
    return get_field<uint32_t>(data.begin(), data.end(), 0);
}

std::ostream& wotreplay::operator<<(std::ostream& os, const packet_t &packet) {
    return os << to_string(packet);
}

std::string wotreplay::to_string(const packet_t &packet) {
    std::stringstream result;
    for (auto val : packet.get_data()) {
        result << (boost::format("0x%1$02X ") % (uint32_t) val).str();
    }
    return result.str();
}