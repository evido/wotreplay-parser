#include "packet.h"

using namespace wotreplay;

packet_t::packet_t(const slice_t &data) {
    this->set_data(data);
}

uint8_t packet_t::type() const {
    assert(has_property(property_t::type));
    return get_field<uint8_t>(data.begin(), data.end(), 1);
}

uint32_t packet_t::player_id() const {
    // assert(has_property(property_t::player_id));
    return get_field<uint32_t>(data.begin(), data.end(), 9);
}

float packet_t::clock() const {
    // assert(has_property(property_t::clock));
    return get_field<float>(data.begin(), data.end(), 5);
}

std::tuple<float, float, float> packet_t::position() const {
    assert(type() == 0x0a);
    float x = get_field<float>(data.begin(), data.end(), 21);
    float y = get_field<float>(data.begin(), data.end(), 25);
    float z = get_field<float>(data.begin(), data.end(), 29);
    return std::make_tuple(x,y,z);
}

std::tuple<float, float, float> packet_t::hull_orientation() const {
    assert(has_property(property_t::hull_orientation));
    float x = get_field<float>(data.begin(), data.end(), 45);
    float y = get_field<float>(data.begin(), data.end(), 49);
    float z = get_field<float>(data.begin(), data.end(), 53);
    return std::make_tuple(x,y,z);
}

float packet_t::turret_orientation() const {
    assert(property_t::turret_orientation);
    return get_field<float>(data.begin(), data.end(), 53);
}

uint16_t packet_t::health() const {
    assert(has_property(property_t::health));
    return get_field<uint16_t>(data.begin(), data.end(), 21);
}

const std::array<bool, static_cast<size_t>(property_t::property_nr_items)> &packet_t::get_properties() const {
    return properties;
}

bool packet_t::has_property(property_t p) const {
    return properties[static_cast<size_t>(p)];
}

void packet_t::set_data(const slice_t &data) {
    this->data = data;
    std::fill(properties.begin(), properties.end(), false);
    switch(get_field<uint8_t>(data.begin(), data.end(), 1)) {
        case 0x0a:
            properties[static_cast<size_t>(property_t::position)] = true;
            properties[static_cast<size_t>(property_t::hull_orientation)] = true;
            properties[static_cast<size_t>(property_t::type)] = true;
            properties[static_cast<size_t>(property_t::clock)] = true;
            properties[static_cast<size_t>(property_t::player_id)] = true;
            break;
        case 0x07: {
            properties[static_cast<size_t>(property_t::type)] = true;
            properties[static_cast<size_t>(property_t::clock)] = true;
            properties[static_cast<size_t>(property_t::player_id)] = true;
            properties[static_cast<size_t>(property_t::is_shot)] = true;
            uint8_t sub_type = get_field<uint8_t>(data.begin(), data.end(), 13);
            properties[static_cast<size_t>(property_t::health)] = sub_type == 0x04;
            break;
        }
        case 0x08: {
            if (data.size() > 25) {
                auto signature = get_field<uint32_t>(data.begin(), data.end(), 21);
                properties[static_cast<size_t>(property_t::tank_destroyed)] = 0x02801006 == signature;
            }
            properties[static_cast<size_t>(property_t::clock)] = true;
            properties[static_cast<size_t>(property_t::player_id)] = true;
            properties[static_cast<size_t>(property_t::type)] = true;
            uint8_t sub_type = get_field<uint8_t>(data.begin(), data.end(), 13);
            switch (sub_type) {
                case 0x02:
                    properties[static_cast<size_t>(property_t::health)] = true;
                    properties[static_cast<size_t>(property_t::source)] = true;
                    break;
                case 0x19:
                    // related to tank destroyed
                    break;
            }
            break;
        }
        case 0x1F: {
            properties[static_cast<size_t>(property_t::type)] = true;
            properties[static_cast<size_t>(property_t::clock)] = true;
            properties[static_cast<size_t>(property_t::message)] = true;
        }
        default:
            if (data.size() >= 9) {
                properties[static_cast<size_t>(property_t::clock)] = true;
            }
            properties[static_cast<size_t>(property_t::type)] = true;
            break;
    }
}

uint32_t packet_t::source() const {
    assert(has_property(property_t::source));
    return get_field<uint32_t>(data.begin(), data.end(), 23);
}

const slice_t &packet_t::get_data() const {
    return data;
}

std::tuple<uint32_t, uint32_t> packet_t::tank_destroyed() const {
    assert(has_property(property_t::tank_destroyed));
    return std::make_tuple(
        get_field<uint32_t>(data.begin(), data.end(), 26),
        get_field<uint32_t>(data.begin(), data.end(), 31)
    );
}

std::string packet_t::message() const {
    size_t field_size = get_field<uint32_t>(data.begin(), data.end(), 9);
    return std::string(data.begin() + 13, data.begin() + 13 + field_size);
}

void wotreplay::display_packet(const packet_t &packet) {
    for (auto val : packet.get_data()) {
        printf("%02X ", static_cast<unsigned>(val));
    }
    printf("\n");
}
