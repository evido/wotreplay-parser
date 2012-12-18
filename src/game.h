#ifndef wotreplay_game_h
#define wotreplay_game_h

#include "packet.h"
#include "types.h"

#include <set>
#include <vector>

/** @file */

namespace wotreplay {
    /**
     * An object wrapping the properties of the game with the actions
     * by the players in the game represented by a list of packets.
     */
    class game_t {
        friend class parser_t;
    public:
        /**
         * Returns the game mode of this game, the possible values are:
         * ass = assault, dom = domination (encounter), ctf = default.
         * @return The game mode
         */
        const std::string &get_game_mode() const;
        /**
         * @return The map name
         */
        const std::string &get_map_name() const;
        /**
         * Returns the data block 'replay' containing a sequence of packets describing the actions in the game. This method
         * is not supported for replays before version 0.7.2.
         * @return Data block 'replay'
         */
        const buffer_t &get_raw_replay() const;
        /**
         * @return A collection with all the packets of this game.
         */
        const std::vector<packet_t> &get_packets() const;
        /**
         * Get the players associated with a team.
         * @param team_id The team for which to get the players
         * @return A collection with the player id's associated with a team.
         */
        const std::set<int> &get_team(int team_id) const;
        /**
         * Describes the boundaries of the current map in the following order:
         * { x_min, x_max, y_min, y_max }
         * @return The map boundaries
         */
        const std::array<int, 4> &get_map_boundaries() const;
         /** 
          * The player id recording this game.
          * @return the player id of the recorder of this game
          */
        uint32_t get_recorder_id() const;
        /**
         * Tries to find the required property which is the closest (with respect to \c clock) to the specified packet. This method uses the properties
         * \c property::player_id and \c property_t::clock from the packet referenced by packet_id.
         * @param clock the reference clock to determine the distance of the packet
         * @param player_id the player id for which to find a packet with a property
         * @param property required property of the packet to be found
         * @param out output variable containing the packet if one is found
         * @return \c true if a packet was found, \c false if no packet with the required properties was found
         */
        bool find_property(uint32_t clock, uint32_t player_id, property_t property, packet_t &out) const;
        /**
         * Determine team for a player.
         * @param player_id The player_id for which to determine the team.
         * @return The team-id for the given player-id.
         */
        int get_team_id(int player_id) const;
        /**
         * Returns the version string as stored in the replay file.
         * @return The version string as stored in the replay file.
         */
        const std::string& get_version() const;
        /**
         * Returns the data block 'game begin' containing a JSON string describing the start of the game.
         * @return Data block 'game begin'
         */
        const buffer_t &get_game_begin() const;
        /**
         * Returns the data block 'game end' containing a JSON string describing the end
         * of the game. . This method is not supported for replays before version 0.7.2.
         * @return Data block 'game end'
         */
        const buffer_t &get_game_end() const;
    private:
        std::vector<packet_t> packets;
        std::array<int, 4> map_boundaries;
        std::array<std::set<int>, 2> teams;
        std::string game_mode;
        std::string map_name;
        std::string version;
        buffer_t game_begin;
        buffer_t game_end;
        buffer_t replay;
        uint32_t recorder_id;
    };

    /**
     * @fn void wotreplay::write_parts_to_file(const game_t &game)
     * Writes each data block of a replay file to a file, for debugging purpopses.
     * @param game The parsed replay.
     */
    void write_parts_to_file(const game_t &game);

    /**
     * @fn std::tuple<float, float> get_2d_coord(const std::tuple<float, float, float> &position, const game_t &game, int width, int height)
     * @brief Get 2D Coordinates from a WOT position scaled to given width and height.
     * @param position The position to convert to a 2d coordinate
     * @param game_info game_info object containing the boundaries of the map
     * @param width target map width to scale the coordinates too
     * @param height target map height to scale the coordinates too
     * @return The scaled 2d coordinates
     */
    std::tuple<float, float> get_2d_coord(const std::tuple<float, float, float> &position, const game_t &game_info, int width, int height);

    /**
     * @fn void wotreplay::show_map_boundaries(const game_t &game, const std::vector<packet_t> &packets)
     * Prints the position boundaries reached by any player in the map.
     * @param game game object containing the team information
     * @param packets a list of all the packets in a replay
     */
    void show_map_boundaries(const game_t &game, const std::vector<packet_t> &packets);
}

#endif /* defined(wotreplay_game_h) */
