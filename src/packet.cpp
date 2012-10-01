/*
 Copyright (c) 2012, Jan Temmerman
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the <organization> nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL Jan Temmerman BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "packet.h"

using namespace wotreplay;

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

float packet_t::clock() const {
    assert(has_property(property::clock));
    return get_field<float>(data.begin(), data.end(), 5);
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
            properties[property::is_shot] = true;
            uint8_t sub_type = get_field<uint8_t>(data.begin(), data.end(), 13);
            properties[property::health] = sub_type == 0x02;
            break;
        }
        case 0x08: {
            if (data.size() > 25) {
                auto signature = get_field<uint32_t>(data.begin(), data.end(), 21);
                properties[property::tank_destroyed] = 0x02801006 == signature;
            }

            properties[property::clock] = true;
            properties[property::player_id] = true;
            properties[property::type] = true;
            break;
        }
        default:
            if (data.size() >= 13) {
                properties[property::player_id] = true;
            }
            if (data.size() >= 9) {
                properties[property::clock] = true;
            }
            properties[property::type] = true;
            break;
    }
}

const slice_t &packet_t::get_data() const {
    return data;
}

std::tuple<uint32_t, uint32_t> packet_t::tank_destroyed() const {
    assert(has_property(property::tank_destroyed));
    return std::make_tuple(
        get_field<uint32_t>(data.begin(), data.end(), 26),
        get_field<uint32_t>(data.begin(), data.end(), 31)
    );
}

void wotreplay::display_packet(const packet_t &packet) {
    for (auto val : packet.get_data()) {
        unsigned ival = (unsigned)(unsigned char)(val);
        printf("%02X ", ival);
    }
    printf("\n");
}

