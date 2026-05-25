#include "packet.h"
#include "types.h"

#include <boost/format.hpp>
#include <cassert>
#include <cstdint>
#include <numbers>
#include <sstream>
#include <stdexcept>

using namespace wotreplay;

packet_t::packet_t(const slice_t &data, bool blitz_packet) : blitz_packet(blitz_packet) { this->set_data(data); }

uint32_t packet_t::type() const {
    assert(has_property(property_t::type));
    return get_field<uint32_t>(data.begin(), data.end(), 4);
}

// type=0x01
//  0: 76 00 00 00 01 00 00 00
//  8: 6E 16 CF 3D A2 CB 1F 00
// 16: FF 0B 00 00 00 00 00 00
// 24: 63 8E 19 43 B5 CA 47 42
// 32: 30 5D 57 43 D0 B9 01 C0
// 40: 00 00 00 00 00 00 00 00
// 48: 4E 00 00 00 02 D6 DC 1D 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00
// 0A FB 14 00 00 6A 49 00 00 6A 4A 00 00 FB 0E 00 00 FB 12 00 00 6A 48 00 00 FD
// 1D 00 00 F8 4D 02 00 FD 15 00 00 FD 13 00 00 00 00 00 00 00 00 00 00 00 00 00
// 00 00 00 00 00 00
uint32_t packet_t::recorder_id() const {
    assert(has_property(property_t::recorder_id));

    return get_field<uint32_t>(data.begin(), data.end(), 53);
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
    assert(has_property(property_t::position));

    if (this->blitz_packet) {
        float x = get_field<float>(data.begin(), data.end(), 24);
        float y = get_field<float>(data.begin(), data.end(), 28);
        float z = get_field<float>(data.begin(), data.end(), 32);

        return std::make_tuple(x, y, z);
    } else {
        float x = get_field<float>(data.begin(), data.end(), 20);
        float y = get_field<float>(data.begin(), data.end(), 24);
        float z = get_field<float>(data.begin(), data.end(), 28);

        return std::make_tuple(x, y, z);
    }
}

float packet_t::direction() const {
    assert(has_property(property_t::position));
    return get_field<float>(data.begin(), data.end(), 36);
}

// 00: 31 00 00 00
// 04: 0A 00 00 00
// 08: 44 7B 80 43
// 12: BD 31 15 10
// 16: 1E 07 00 00
// 20: 00 00 00 00
// 24: 90 68 50 C2 58 5A 96 41 84 B5 61 C3
// 36: 79 7B 39 00 00 00 37 36 79 FB 37 48
// 48: 0B A5 BF 72 58 2F BE C2 CA A1 BD 01
std::tuple<float, float, float> packet_t::rotation() const {
    assert(has_property(property_t::position));
    float roll = get_field<float>(data.begin(), data.end(), 40);
    float pitch = get_field<float>(data.begin(), data.end(), 44);
    float yaw = get_field<float>(data.begin(), data.end(), 48);
    return std::make_tuple(roll, pitch, yaw);
}

float packet_t::hull_orientation2() const { return get_field<float>(data.begin(), data.end(), 48); }

float packet_t::turret_orientation() const {
    assert(property_t::turret_orientation);
    return get_field<uint16_t>(data.begin(), data.end(), 24) * std::numbers::pi / 32767;
}

uint16_t packet_t::health() const {
    assert(has_property(property_t::health));

    if (type() == 0x07 || type() == 0x08) {
        return get_field<uint16_t>(data.begin(), data.end(), 24);
    } else if (type() == 0x05) {
        return get_field<uint16_t>(data.begin(), data.end(), 63);
    } else {
        throw std::runtime_error("unknown health position");
    }
}

const std::array<bool, static_cast<size_t>(property_t::property_nr_items)> &packet_t::get_properties() const { return properties; }

bool packet_t::has_property(property_t p) const { return properties[static_cast<size_t>(p)]; }

void packet_t::set_data(const slice_t &data) {
    this->data = data;

    // reset all properties
    std::fill(properties.begin(), properties.end(), false);

    // enable default properties
    properties[static_cast<size_t>(property_t::type)] = true;
    properties[static_cast<size_t>(property_t::length)] = true;

    switch (get_field<uint32_t>(data.begin(), data.end(), 4)) {
    case 0x01:
        properties[static_cast<size_t>(property_t::clock)] = true;
        properties[static_cast<size_t>(property_t::player_id)] = true;
        properties[static_cast<size_t>(property_t::recorder_id)] = true;
        break;
    case 0x03:
    case 0x05:
        properties[static_cast<size_t>(property_t::clock)] = true;
        properties[static_cast<size_t>(property_t::player_id)] = true;
        properties[static_cast<size_t>(property_t::sub_type)] = true;
        properties[static_cast<size_t>(property_t::player_name)] = sub_type() == 0x02;
        properties[static_cast<size_t>(property_t::health)] = sub_type() == 0x02;
        properties[static_cast<size_t>(property_t::max_health)] = sub_type() == 0x02;
        break;
    case 0x0a:
        properties[static_cast<size_t>(property_t::position)] = true;
        properties[static_cast<size_t>(property_t::hull_orientation)] = true;
        properties[static_cast<size_t>(property_t::clock)] = true;
        properties[static_cast<size_t>(property_t::player_id)] = true;
        break;
    case 0x0b:
        // position 30 looks like some payload length
        properties[static_cast<size_t>(property_t::map_name)] = data[30] > 1;
        // pass check
        properties[static_cast<size_t>(property_t::clock)] = true;
        break;
    case 0x07: {
        properties[static_cast<size_t>(property_t::clock)] = true;
        properties[static_cast<size_t>(property_t::player_id)] = true;
        properties[static_cast<size_t>(property_t::sub_type)] = true;
        properties[static_cast<size_t>(property_t::turret_orientation)] = sub_type() == 0x02;
        properties[static_cast<size_t>(property_t::health)] = sub_type() == 0x03;
        // properties[static_cast<size_t>(property_t::health)] = sub_type() ==
        // 0x05; properties[static_cast<size_t>(property_t::destroyed_track_id)]
        // = sub_type() == 0x07;
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
            // properties[static_cast<size_t>(property_t::source)] = true;
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
        case 0x30:
            // < 8.5
            properties[static_cast<size_t>(property_t::health)] = true;
            properties[static_cast<size_t>(property_t::source)] = true;
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

std::tuple<uint32_t, uint32_t> find_field5_offset(const packet_t &packet, uint8_t sub_field_index) {
    assert(packet.type() == 0x05 && packet.sub_type() == 0x02);

    const uint32_t field_base = 55;

    const int32_t field_sizes[] = {
        2, 2, 3, 3, 3, -5, -6, 2, 2, 2, -10, 5, 2, 3, 3, 9, 2, 2,
    };

    uint32_t field_index = 0;
    uint32_t field_offset = field_base;
    for (int i = 0; i < sizeof(field_sizes) / sizeof(field_sizes[0]); i += 1) {
        assert(packet.get_data_field<int8_t>(field_offset) == i);

        int field_size = field_sizes[i];

        switch (i) {
        case 0x05:
            field_size = 23 + packet.get_data_field<int8_t>(field_offset + 1);
            break;
        case 0x06:
            field_size = 2 + packet.get_data_field<int8_t>(field_offset + 1) * 16;
            break;
        case 0x0A:
            field_size = 2 + packet.get_data_field<int8_t>(field_offset + 1) * 14;
            break;
        case 0x0B:
            field_size = 2 + packet.get_data_field<int8_t>(field_offset + 1);
            break;
        default:
            field_size = field_sizes[i];
            break;
        };

        if (field_index == sub_field_index) {
            return std::make_tuple(field_offset, field_size);
        }

        field_offset += field_size;
        field_index += 1;
    }

    throw std::runtime_error(std::format("unknown field {}", sub_field_index));
}

uint16_t packet_t::max_health() const {
    assert(has_property(property_t::max_health));
    return get_data_field<uint16_t>(get<0>(find_field5_offset(*this, 0x0D)) + 1);
}

uint32_t packet_t::sub_type() const {
    assert(has_property(property_t::sub_type));
    return get_field<uint8_t>(data.begin(), data.end(), 16);
}

uint8_t packet_t::destroyed_track_id() const {
    assert(has_property(property_t::destroyed_track_id));
    uint8_t destroyed_track_id = 0;
    switch (type()) {
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

std::string packet_t::map_name() const {
    assert(has_property(property_t::map_name));

    std::string packet_data(data.begin(), data.end());
    auto pos = packet_data.rfind("\x80\x3f");

    assert(pos != std::string::npos);

    return packet_data.substr(pos + 2);
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

const slice_t &packet_t::get_data() const { return data; }

std::tuple<uint32_t, uint32_t, uint8_t> packet_t::tank_destroyed() const {
    assert(has_property(property_t::tank_destroyed));
    return std::make_tuple(get_field<uint32_t>(data.begin(), data.end(), 30), get_field<uint32_t>(data.begin(), data.end(), 35),
                           get_field<uint8_t>(data.begin(), data.end(), 42));
}

std::string packet_t::message() const {
    size_t field_size = get_field<uint32_t>(data.begin(), data.end(), 12);
    return std::string(data.begin() + 16, data.begin() + 16 + field_size);
}

std::string packet_t::player_name() const {
    assert(has_property(property_t::player_name));
    size_t field_size = this->get_data_field<uint8_t>(69);
    return std::string(data.begin() + 70, data.begin() + 70 + field_size);
}

int8_t packet_t::team_id() const {
    assert(has_property(property_t::player_name));
    return this->get_data_field<uint8_t>(66) > 0 ? 2 : 1;
}

uint32_t packet_t::length() const {
    assert(has_property(property_t::length));
    return get_field<uint32_t>(data.begin(), data.end(), 0);
}

std::ostream &wotreplay::operator<<(std::ostream &os, const packet_t &packet) { return os << to_string(packet); }

std::ostream &wotreplay::operator<<(std::ostream &os, const slice_t &slice) { return os << to_string(slice); }

std::string wotreplay::to_string(const packet_t &packet) {
    std::stringstream result;

    result << "[ ";
    for (auto val : packet.get_data()) {
        result << (boost::format("%1$02X ") % (uint32_t)val).str();
    }
    result << "]";
    return result.str();
}

std::string wotreplay::to_string(const slice_t &slice) {
    std::stringstream result;

    result << "[ ";
    for (auto val : slice) {
        result << (boost::format("%1$02X ") % (uint32_t)val).str();
    }
    result << "]";
    return result.str();
}
