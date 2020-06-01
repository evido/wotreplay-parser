#ifndef wotreplay_game_h
#define wotreplay_game_h

#include "arena.h"
#include "packet.h"
#include "types.h"

#include <set>
#include <vector>

/** @file */

namespace wotreplay {
	enum game_title_t {
		world_of_tanks,
		world_of_warships
	};
	
    /**
     * The game version of a replay file
     */
    struct version_t {
        version_t() = default;
        /**
         * @param version_string the string representation of the game version
         */
        version_t(const std::string &version_string);
        /** 
         * The major version number
         */
        int major;
        /**
         * The minor version number
         */
        int minor;
        /**
         * The string representation of the version
         */
        std::string text;
    };

    /**
     * Player information
     */
    struct player_t {
        uint32_t player_id;
        int team;
        std::string name;
        std::string tank;
    };

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
         * Get the arena definition
         * @return The map boundaries
         */
        const arena_t &get_arena() const;
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
        const version_t& get_version() const;
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
        /**
         * Get player information with the player id
         * @return Player information
         */
        const player_t &get_player(int player_id) const;
		game_title_t get_game_title() const;
    private:
        std::vector<packet_t> packets;
        std::array<std::set<int>, 2> teams;
        std::string game_mode;
        arena_t arena;
        buffer_t game_begin;
        buffer_t player_info;
        buffer_t game_end;
        buffer_t replay;
        uint32_t recorder_id;
        version_t version;
        std::map<int, player_t> players;
		game_title_t title;
    };

    /**
     * @fn void wotreplay::write_parts_to_file(const game_t &game)
     * Writes each data block of a replay file to a file, for debugging purpopses.
     * @param game The parsed replay.
     */
    void write_parts_to_file(const game_t &game);

    /**
     * @fn std::tuple<float, float> get_2d_coord(const std::tuple<float, float, float> &position, const bounding_box_t &bounding_box, int width, int height)
     * @brief Get 2D Coordinates from a WOT position scaled to given width and height.
     * @param position The position to convert to a 2d coordinate
     * @param bounding_box the boundaries of the map of the map
     * @param width target map width to scale the coordinates too
     * @param height target map height to scale the coordinates too
     * @return The scaled 2d coordinates
     */
    std::tuple<float, float> get_2d_coord(const std::tuple<float, float, float> &position, const bounding_box_t &bounding_box, int width, int height);

    /**
     * @fn void wotreplay::show_map_boundaries(const game_t &game, const std::vector<packet_t> &packets)
     * Prints the position boundaries reached by any player in the map.
     * @param game game object containing the team information
     * @param packets a list of all the packets in a replay
     */
    void show_map_boundaries(const game_t &game, const std::vector<packet_t> &packets);

    /**
     * @fn int wotreplay::get_start_packet(const game_t &game, double skip)
     * Determine offset of first packet a number of seconds after the game has started
     * @param game game information
     * @param skip number of seconds since start of game
     * @return offset for first packet
     */
    int get_start_packet (const game_t &game, double skip);

    /**
     * @fn double wotreplay::dist(const std::tuple<float, float, float> &begin, const std::tuple<float, float, float> &end)
     * Calculate the distance between two points
     * @param begin starting point
     * @param end end point
     * @return distance
     */
    double dist(const std::tuple<float, float, float> &begin,
                const std::tuple<float, float, float> &end);
}

#endif /* defined(wotreplay_game_h) */
