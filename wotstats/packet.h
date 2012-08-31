//
//  packet.h
//  wotstats
//
//  Created by Jan Temmerman on 31/08/12.
//
//

#ifndef __wotstats__packet__
#define __wotstats__packet__

#include <tuple>
#include <vector>

#include <stdint.h>

#include "types.h"

namespace wotstats {

    enum property {
        clock,
        gun_direction,
        fired_shot,
        health,
        is_shot,
        position,
        player_id,
        sub_type,
        type,
        turret_direction,
    };

    struct packet_t {
        uint8_t type() const;
        uint32_t clock() const;
        uint32_t player_id() const;
        std::tuple<float, float, float> position() const;
        std::tuple<float, float, float> gun_direction() const;
        std::tuple<float, float, float> turret_direction() const;
        uint16_t health() const;
        bool is_shot() const;
        bool fired_shot() const;
        std::vector<property> get_properties() const;
        bool has_property(property p) const;
        slice_t data;
    };

    template <typename U, typename T>
    const U &get_field(T begin, T end, size_t offset) {
        assert((offset + sizeof(U)) < std::distance(begin, end));
        return *reinterpret_cast<const U*>(&*(begin + offset));
    }
}

#endif /* defined(__wotstats__packet__) */
