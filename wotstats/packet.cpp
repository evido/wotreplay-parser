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

const std::vector<property> &packet_t::get_properties() const {
    return properties;
}

bool packet_t::has_property(property p) const {
    std::vector<property> properties = get_properties();
    return std::find(properties.begin(), properties.end(), p) != properties.end();
}

void packet_t::set_data(const slice_t &data) {
    this->data = data;
    switch(get_field<uint8_t>(data.begin(), data.end(), 1)) {
        case 0x0a:
            properties.push_back(property::position);
            properties.push_back(property::type);
            properties.push_back(property::clock);
            properties.push_back(property::player_id);
            break;
        case 0x07: {
            properties.push_back(property::type);
            properties.push_back(property::clock);            
            properties.push_back(property::player_id);
            uint8_t sub_type = get_field<uint8_t>(data.begin(), data.end(), 13);
            if (sub_type == 0x02) {
                properties.push_back(property::health);
            }
        }
        default:
            if (data.size() >= 12) {
                properties.push_back(property::clock);
                properties.push_back(property::player_id);
            }
            properties.push_back(property::type);
            break;
    }
}

const slice_t &packet_t::get_data() const {
    return data;
}