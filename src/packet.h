#ifndef wotreplay_packet_h
#define wotreplay_packet_h

#include "types.h"

#include <algorithm>
#include <array>
#include <stdint.h>
#include <tuple>

/** @file packet.h */

namespace wotreplay {
    
    /**
     * @enum wotreplay::property_t
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
        source,
        hull_orientation,
        turret_orientation,
        message,
        target,
        destroyed_track_id,
        alt_track_state,
        length,
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
        /** total packet payload length */
        uint32_t length() const;
        /** @return The packet type */
        uint32_t type() const;
        /** @return The clock value of this packet. */
        float clock() const;
        /** @return The player_id value of this packet. */
        uint32_t player_id() const;
        /** @return The position value of this packet. */
        std::tuple<float, float, float> position() const;
        /** @return The hull orentation value of this packet. */
        std::tuple<float, float, float> hull_orientation() const;
        /** @return The turret orentation value of this packet. */
        float turret_orientation() const;
        /** @return The remaining health of a player. */
        uint16_t health() const;
        /** @return The remaining health update source of a player. */
        uint32_t source() const;
        /** @return Indicates the player_id was hit. */
        bool is_shot() const;
        /** @return A tuple of with the player_id's of the target and the killer. */
        std::tuple<uint32_t, uint32_t, uint8_t> tank_destroyed() const;
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
        /**
         * Find a binary value in this packet.
         * @param value The value to be found in this packet.
         * @return \c true if the packet contains the value, \c false if not
         */
        template <typename T>
        bool find(const T &value) const {
            const uint8_t *arr = reinterpret_cast<const uint8_t*>(&value);
            return std::search(data.begin(), data.end(), arr, arr + sizeof(value)) != data.end();
        }
        /**
         * Get packet sub type, relevant for packet type 0x08 and 0x07
         */
        uint32_t sub_type() const;
        /**
         * contains a message for the battle log formatted as a html element
         * example: \verbatim <font color='#DA0400'>SmurfingBird[RDDTX] (VK 36.01 H)&nbsp;:&nbsp;</font><font color='#FFFFFF'>so far so good</font> \endverbatim
         * @return the message contained by this packet
         */
        std::string message() const;
        /**
         * Target information, object of the actin in 0x08 / 0x1B packets
         * @return get target player id
         */
        uint32_t target() const;
        /**
         * get the id of the destroyed track, 0x1D for left track, 0x1E for right track
         * @return id of the destroyed track
         */
        uint8_t destroyed_track_id() const;
        /**
         * get status of alternative track, 0xFO indicates the track works, 0xF6 indicates the track is broken
         * @return alt track state
         */
        uint8_t alt_track_state() const;
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
        assert((offset + sizeof(U)) <= std::distance(begin, end));
        return *reinterpret_cast<const U*>(&*(begin + offset));
    }

    /**
     * @fn std::string to_string(const packet_t &packet)
     * @brief string representation of the packet
     * @param packet target packet
     */
    std::string to_string(const packet_t &packet);

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

    /**
     * serialize packet to given stream
     * @param os target stream
     * @param packet packet to write to the stream
     */
    std::ostream& operator<<(std::ostream& os, const packet_t &packet);
}

#endif /* defined(wotreplay_packet_h) */
