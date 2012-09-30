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

#ifndef wotreplay_parser_packet_h
#define wotreplay_parser_packet_h

#include "types.h"

#include <tuple>
#include <array>
#include <stdint.h>

namespace wotstats {

    enum property {
        clock = 0,
        gun_direction,
        fired_shot,
        health,
        is_shot,
        position,
        player_id,
        sub_type,
        type,
        turret_direction,
        tank_destroyed,
        property_nr_items
    };

    class packet_t {
    public:
        // constructors
        packet_t() = default;
        packet_t(const slice_t &slice_t);

        // property accessor
        uint8_t type() const;
        float clock() const;
        uint32_t player_id() const;
        std::tuple<float, float, float> position() const;
        std::tuple<float, float, float> gun_direction() const;
        std::tuple<float, float, float> turret_direction() const;
        uint16_t health() const;
        bool is_shot() const;
        bool fired_shot() const;
        /* target , killer */
        std::tuple<uint32_t, uint32_t> tank_destroyed() const;
        // property helper methods
        const std::array<bool, property_nr_items> &get_properties() const;
        bool has_property(property p) const;

        // modify / retrieve packet data
        void set_data(const slice_t &data);
        const slice_t &get_data() const;
    private:
        std::array<bool, property_nr_items> properties;
        slice_t data;
    };

    template <typename U, typename T>
    const U &get_field(T begin, T end, size_t offset) {
        assert((offset + sizeof(U)) < std::distance(begin, end));
        return *reinterpret_cast<const U*>(&*(begin + offset));
    }
}

#endif /* defined(__wotstats__packet__) */
