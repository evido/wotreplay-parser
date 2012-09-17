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
#include <array>
#include <stdint.h>

#include "types.h"

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
        packet_t() = default;
        packet_t(const slice_t &slice_t);

        // property accessor
        uint8_t type() const;
        uint32_t clock() const;
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
