#ifndef wotreplay_packet_h
#define wotreplay_packet_h

#include "types.h"

#include <tuple>
#include <array>
#include <stdint.h>

/** @file packet.h */

namespace wotreplay {
    /**
     * @enum wot::property_t
     * @brief A list of detectable properties in packets of a wot:replay_file.
     */
    enum property_t {
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
        const std::array<bool, static_cast<size_t>(property_t::property_nr_items)> &get_properties() const;
        /**
         * Determines if the packet has the property specified.
         * @param p The property
         * @return \c true if the packet contains the property, \c false if not.
         */
        bool has_property(property_t p) const;

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
        std::array<bool, static_cast<size_t>(property_t::property_nr_items)> properties;
        /** The data content of this packet. */
        slice_t data;
    };

    /**
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

    /**
     * @fn void display_packet(const packet_t &packet)
     * @brief Prints the bytes of a packet.
     * @param packet The packet to print out.
     */
    void display_packet(const packet_t &packet);



    /**
     * @fn bool find_property(const std::vector<packet_t> &packets, uint32_t clock, uint32_t player_id, property_t property, packet_t &out)
     * Tries to find the required property which is the closest (with respect to \c clock) to the specified packet. This method uses the properties
     * \c property::player_id and \c property_t::clock from the packet referenced by packet_id.
     * @param clock the reference clock to determine the distance of the packet
     * @param player_id the player id for which to find a packet with a property
     * @param property required property of the packet to be found
     * @param out output variable containing the packet if one is found
     * @return \c true if a packet was found, \c false if no packet with the required properties was found
     */
    bool find_property(const std::vector<packet_t> &packets, uint32_t clock, uint32_t player_id, property_t property, packet_t &out);
}

#endif /* defined(wotreplay_packet_h) */
