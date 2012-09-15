//
//  packet.cpp
//  wotstats
//
//  Created by Jan Temmerman on 31/08/12.
//
//

#include "packet.h"

using namespace wotstats;

packet_t::packet_t(const slice_t &data) {
    this->set_data(data);
}

uint8_t packet_t::type() const {
    assert(has_property(property::type));
    return get_field<uint8_t>(data.begin(), data.end(), 1);
}

uint32_t packet_t::player_id() const {
    assert(has_property(property::player_id));
    return get_field<uint32_t>(data.begin(), data.end(), 9);
}

uint32_t packet_t::clock() const {
    assert(has_property(property::clock));
    return get_field<uint32_t>(data.begin(), data.end(), 5);
}

std::tuple<float, float, float> packet_t::position() const {
    assert(type() == 0x0a);
    float x = get_field<float>(data.begin(), data.end(), 21);
    float y = get_field<float>(data.begin(), data.end(), 25);
    float z = get_field<float>(data.begin(), data.end(), 29);
    return std::make_tuple(x,y,z);
}

uint16_t packet_t::health() const {
    assert(has_property(property::health));
    return get_field<uint16_t>(data.begin(), data.end(), 21);
}

const std::array<bool, property_nr_items> &packet_t::get_properties() const {
    return properties;
}

bool packet_t::has_property(property p) const {
    return properties[p];
}

void packet_t::set_data(const slice_t &data) {
    this->data = data;
    std::fill(properties.begin(), properties.end(), false);
    switch(get_field<uint8_t>(data.begin(), data.end(), 1)) {
        case 0x0a:
            properties[property::position] = true;
            properties[property::type] = true;
            properties[property::clock] = true;
            properties[property::player_id] = true;
            break;
        case 0x07: {
            properties[property::type] = true;
            properties[property::clock] = true;
            properties[property::player_id] = true;
            uint8_t sub_type = get_field<uint8_t>(data.begin(), data.end(), 13);
            if (sub_type == 0x02) {
                properties[property::health] = true;
            }
        }
        default:
            if (data.size() >= 12) {
                properties[property::clock];
                properties[property::player_id];
            }
            properties[property::type] = true;
            break;
    }
}

const slice_t &packet_t::get_data() const {
    return data;
}