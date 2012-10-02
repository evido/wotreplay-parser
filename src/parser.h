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

#ifndef wotreplay_parser_h
#define wotreplay_parser_h

#include <boost/filesystem.hpp>
#include <iostream>
#include <set>
#include <vector>

#include "types.h"
#include "packet.h"

/** @file */

namespace wotreplay {
    /** game_info describes a minimal number of properties from the replay file. */
    struct game_info {
        /** The name of the map. */
        std::string map_name;
        /** String containing the game_mode game mode: ass = assault, dom = domination (encounter), ctf = default. */
        std::string game_mode;
        /** Player id's seperated per team */
        std::array<std::set<int>, 2> teams;
        /** The player id recording this game. */
        unsigned recorder_id;
        /** Describes the boundaries of the current map: the first element describes the boundaries of the x-axis, the second element describes the boundaries of the y-axis */
        std::array<std::array<int, 2>, 2> boundaries;
        /** Relative path to the mini map associated with this replay */
        std::string mini_map;
    };

    /** wotreplay::parser is a class responsible for parsing a World of Tanks replay file.  */
    class parser {
        /**
         * @brief Validate the inner workings of the class parser
         * Validate the inner workings of the class parser by:
         * -# checking the number of unread bytes
         * -# validating the parsed packets with the game metadata
         * @param path The directory which contains the replays to validate the parser with
         */
        friend void validate_parser(const std::string &path);
    public:
        /** 
         * Default constructor for creating an empty replay_file 
         */
        parser() = default;
        /**
         * Constructor for creating a replay_file from a valid std::inputstream.
         * The inputstream will be consumed completely after returning from this method.
         * @param is The inputstream containing the contest of a wot replay file.
         * @param debug print diagnostic messages
         */
        parser(std::istream &is, bool debug = false);
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
        /**
         * Returns the data block 'replay' containing a sequence of packets describing the actions in the game. This method
         * is not supported for replays before version 0.7.2.
         * @return Data block 'replay'
         */
        const buffer_t &get_replay() const;
        /**
         * Returns a game_info object describing a minimal number of properties of the replay.
         * @return game_info object
         */
        const game_info &get_game_info() const;
        /**
         * Returns a list with all the recorded and decoded packets.
         * @return A list with all the packets.
         */
        const std::vector<packet_t> &get_packets() const;
        /** 
         * Tries to find the required property which is the closest (with respect to \c clock) to the specified packet. This method uses the properties
         * \c property::player_id and \c property::clock from the packet referenced by packet_id.
         * @param packet_id index of the packet in the packet list
         * @param property required property of the packet to be found
         * @param out output variable containing the packet if one is found
         * @return \c true if a packet was found, \c false if no packet with the required properties was found
         * @deprecated use find_property(uint32_t clock, uint32_t player_id, property property, packet_t &out) const instead
         */ 
        bool find_property(size_t packet_id, property property, packet_t &out) const;
        /**
         * Tries to find the required property which is the closest (with respect to \c clock) to the specified packet. This method uses the properties
         * \c property::player_id and \c property::clock from the packet referenced by packet_id.
         * @param clock the reference clock to determine the distance of the packet
         * @param player_id the player id for which to find a packet with a property
         * @param property required property of the packet to be found
         * @param out output variable containing the packet if one is found
         * @return \c true if a packet was found, \c false if no packet with the required properties was found
         */
        bool find_property(uint32_t clock, uint32_t player_id, property property, packet_t &out) const;
        /**
         * Indicates if the parsed file is a legacy (< 0.7.2) replay file. This means 'game begin', 'game end' will be missing.
         * @return \c true if file is in a legacy format \c false if the file is in the 'new' format.
         */
        bool is_legacy() const;
        /**
         * Parses the replay file from a buffer_t. The argument should contain the complete replay file.
         * @param buffer The complete contents of a replay file.
         */
        void parse(buffer_t &buffer);
        /**
         * Set debug mode for this instance.
         * @param debug The new value for debug.
         */
        void set_debug(bool debug);
        /**
         * @return The debug setting for this parser instance.
         */
        bool get_debug() const;
    private:
        /**
         * Indicates if the passed buffer_t contains a legacy (< 0.7.2) replay file. 
         * @return \c true if file is in a legacy format \c false if the file is in the 'new' format.
         */
        bool is_legacy_replay(const buffer_t &buffer) const;
        /**
         * Extracts each data block from the replay file and adds it the the output variable.
         * @param buffer A buffer_t containing the complete contents of a replay file.
         * @param data_blocks output variable to contain a slice_t to each data block.
         */
        void get_data_blocks(buffer_t &buffer, std::vector<slice_t> &data_blocks) const;
        /**
         * Determines the number of data blcks in the replay file.
         * @param buffer A buffer_t containing the complete contents of a replay file.
         * @return The number of data blocks present in the replay file.
         */
        uint32_t get_data_block_count(const buffer_t &buffer) const;
        /**
         * Performs an in place decryption of the replay using the given key. The decryption
         * is a (broken) variant of CBC decryption and is performed as follows:
         * -# Set the variable previous_block (with size of 16 bytes) to 0
         * -# Decrypt a block with the given key
         * -# XOR the block with the previous (decrypted) block
         * -# Go back to step 2, until there are no more blocks.
         * @param buffer A buffer_t containing the contents of the data block 'replay'.
         * @param key The blowfish key used for decryption.
         */
        void decrypt_replay(buffer_t &buffer, const unsigned char* key);
        /**
         * This method performs an extraction (using zlib) of the given replay. The resulting
         * data is copied to a new buffer.
         * @param compressed_replay The buffer containing the compressed replay
         * @param replay The output variable that will contain the decompressed replay after the execution of this method.
         */
        void extract_replay(buffer_t &compressed_replay, buffer_t &replay);
        // methods helping with packet processing
        template <typename iterator>
        /**
         * Helper method to read a single packet from a begin and end iterator.
         * @param begin iterator to denote the beginning of the packet.
         * @param end iterator to denote the end of the packet.
         * @return The newly constructed packet.
         */
        packet_t read_packet(iterator begin, iterator end);
        /**
         * Helper method to read all the packets contained in the buffer replay_file::replay.
         * @return Returns the number of bytes read.
         */
        size_t read_packets();
        /**
         * Extracts a game_info object from either the 'game begin' data block or parsed directly from the 'replay'
         * data block.
         */
        void read_game_info();
        /** The data block 'game begin' */
        buffer_t game_begin;
        /** The data block 'game end' */
        buffer_t game_end;
        /** The data block 'replay' */
        buffer_t replay;
        /** A list of all the decoded packets. */
        std::vector<packet_t> packets;
        /** The version string. */
        std::string version;
        /** The game_info object. */
        game_info game_info;
        /** The legacy indicator. */
        bool legacy;
        /** The debug indicator */
        bool debug;
    };

    template <typename iterator>
    packet_t parser::read_packet(iterator begin, iterator end) {
        return packet_t(boost::make_iterator_range(begin, end));
    }

    /**
     * Shows a quick overview of the packet count in the replay file.
     * @param packets A list of packets parsed from a replay file.
     */
    void show_packet_summary(const std::vector<packet_t>& packets);

    /**
     * @fn void wotreplay::write_parts_to_file(const parser &replay)
     * Writes each data block of a replay file to a file, for debugging purpopses.
     * @param replay The parsed replay.
     */
    void write_parts_to_file(const parser &replay);

    /**
     * @fn void wotreplay::show_map_boundaries(const game_info &game_info, const std::vector<packet_t> &packets)
     * Prints the position boundaries reached by any player in the map.
     * @param game_info game_info object containing the team information
     * @param packets a list of all the packets in a replay
     */
    void show_map_boundaries(const game_info &game_info, const std::vector<packet_t> &packets);

    void validate_parser(const std::string &path);

    /**
     * @brief Basic function to check if a path might be a replay file.
     * @return @c true if the path is expected to be a valid replay file, @c false if not
     */
    bool is_replayfile(const boost::filesystem::path &p);
}

#endif
