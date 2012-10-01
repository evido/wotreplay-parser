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

/** @file packet.h */

namespace wotreplay {
    /**
     * @enum wot::property
     * @brief A list of detectable properties in packets of a wot:replay_file.
     */
    enum property {
        clock = 0,
        health,
        is_shot,
        position,
        player_id,
        sub_type,
        type,
        tank_destroyed,
        property_nr_items
    };

    /**
     * A representation of a single packet in a World Of Tanks replay. This
     * class provides easy accessor methods to determine the properties of packet,
     * and the corresponding values of these properties.
     */
    class packet_t {
    public:
        /** 
         * Constructor for a default (empty) packet. 
         */
        packet_t() = default;
        /** 
         * Constructor for a packet with defined content.
         * @param data The content of this packet.
         */
        packet_t(const slice_t &data);

        /** @return The packet type */
        uint8_t type() const;
        /** @return The clock value of this packet. */
        float clock() const;
        /** @return The player_id value of this packet. */
        uint32_t player_id() const;
        /** @return The position value of this packet. */
        std::tuple<float, float, float> position() const;
        /** @return The remaining health of a player. */
        uint16_t health() const;
        /** @return Indicates the player_id was hit. */
        bool is_shot() const;
        /** @return A tuple of with the player_id's of the target and the killer. */
        std::tuple<uint32_t, uint32_t> tank_destroyed() const;
        /** @return An array of the properties available in this packet. */
        const std::array<bool, property_nr_items> &get_properties() const;
        /**
         * Determines if the packet has the property specified.
         * @param p The property
         * @return \c true if the packet contains the property, \c false if not.
         */
        bool has_property(property p) const;

        /**
         * Sets internal packet data to the data passed to the method, the properties
         * of this packet are updated accordingly.
         * @param data The new data of this packet.
         */
        void set_data(const slice_t &data);
        /**
         * @return The data of this packet.
         */
        const slice_t &get_data() const;
    private:
        /** An array containing the presence of each property. */
        std::array<bool, property_nr_items> properties;
        /** The data content of this packet. */
        slice_t data;
    };

    /*!
     * @fn template<typename U, typename T> const U & wot::get_field(T begin, T end, size_t offset)
     * @brief Gets a field with the specified type from an iterator range.
     * @param begin Start of the iterator range
     * @param end End of the iterator range
     * @param offset The offset of the value to return in the iterator range
     * @return The requested value.
     */
    template <typename U, typename T>
    const U &get_field(T begin, T end, size_t offset) {
        assert((offset + sizeof(U)) < std::distance(begin, end));
        return *reinterpret_cast<const U*>(&*(begin + offset));
    }
}

#endif
