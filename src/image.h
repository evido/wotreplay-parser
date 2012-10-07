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

#ifndef wotreplay_image_h
#define wotreplay_image_h

#include "packet.h"
#include "parser.h"

#include <boost/multi_array.hpp>
#include <png.h>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

/** @file */

namespace wotreplay {
    /** wotreplay::image_t draws an image from wotreplay::packet_t on a minimap */
    class image_t {
    public:
        /** 
         * Default constructor for wotreplay::image_t. Creates an empty 500x500 image with
         * with the background created from the information in the game information.
         * @param map_name The map name to construct an image for.
         * @param game_mode The game mode to construct an image for.
         */
        image_t(const std::string &map_name, const std::string &game_mode);
        /**
         * Updates the image with the information of the game.
         * @param game The packet containing the information to be processed.
         */
        void update(const game_t &game);
        /**
         * The last step in creating an image. It is called after all packets have
         * been processed. It is illegal to call void update(const packet_t &packet) 
         * or void image_t::finish() again after this method. The only valid methods
         * are void image_t::reset() and void image_t::write_image(const std::string &path);
         */
        void finish();
        /**
         * Resets the image to an empty image, containing only the background specified
         * with the initial game info object.
         */
        void reset();
        /**
         * Writes the contents of this image as a PNG to the specified path.
         * @param path The location of the new image.
         */
        void write_image(const std::string &path);
        /**
         * Merge the contents of an image with this image.
         * @param image target image to merge this image with
         */
        void merge(const image_t &image);
    private:
        /**
         * Load a background image from the combination map name and game mode.
         * @param map_name The map name of the background image.
         * @param game_mode The game mode of the background image.
         */
        void load_base_map(const std::string &map_name, const std::string &game_mode);
        /**
         * Implementation of drawing a 'death' packet on the image.
         * @param packet The packet containing the information to draw.
         * @param game Game
         */
        void draw_death(const packet_t &packet, const game_t &game);
        /**
         * Implementation of drawing a 'position' packet on the image.
         * @param packet The packet containing the information to draw.
         * @param game Game
         */
        void draw_position(const packet_t &packet, const game_t &game, boost::multi_array<float, 3> &image);
        /** Background image */
        boost::multi_array<uint8_t, 3> base;
        /** Image containing the frequency a player is located on a coordinate */
        boost::multi_array<float, 3> positions;
        /** Image containing the frequency a player has died on a coordinate */
        boost::multi_array<float, 3> deaths;
        /** Contains the resulting image. */
        boost::multi_array<uint8_t, 3> result;
        /** A collection of dead players, as seen by the updates from packets */
        std::set<int> dead_players;
    };

    /**
     * @fn int mix(int v0, int v1, float a1, int v2, float a2)
     * Helper function to mix colors.
     * @param v0 base value
     * @param v1 new value 1
     * @param a1 alpha value 1
     * @param v2 new value 2
     * @param a2 alpha value 2
     */
    int mix(int v0, int v1, float a1, int v2, float a2);
    
    /**
     * @fn bool write_png(const std::string &out, png_bytepp image, size_t width, size_t height, bool alpha)
     * @brief Write out an image to path out with given width and height.
     * @param out The path of the new image.
     * @param image The contents of the image.
     * @param width The width of the image.
     * @param height The height of the image.
     * @param alpha Indicates if the image has alpha values.
     * @return @c if the image was written successfully, @c false is not
     */
    bool write_png(const std::string &out, png_bytepp image, size_t width, size_t height, bool alpha);

    /**
     * @fn void read_png(const std::string &in, png_bytepp image, int &width, int &height, int &channels)
     * @brief Wrapper for libpng functions to read a png image.
     * @param in the path of the image to read
     * @param image output variable which will contain the contents of the image
     * @param width the width of the image
     * @param height the height of the image
     * @param channels the number of channels in the image
     */
    void read_png(const std::string &in, png_bytepp image, int &width, int &height, int &channels);

    /**
     * @fn void write_image(const std::string &file_name, boost::multi_array<uint8_t, 3> &image)
     * @brief Wrapper for write_png
     * @param file_name The path of the new image.
     * @param image The contents of the image.
     */
    void write_image(const std::string &file_name, boost::multi_array<uint8_t, 3> &image);

    /**
     * @fn void get_row_pointers(boost::multi_array<uint8_t, 3> &image, std::vector<png_bytep> &row_pointers)
     * @brief Extract row pointers from a boost::multi_array
     * @param row_pointers Out variable to store the row pointers.
     */
    void get_row_pointers(boost::multi_array<uint8_t, 3> &image, std::vector<png_bytep> &row_pointers);

    /**
     * @fn void read_mini_map(const std::string &map_name, const std::string &game_mode, boost::multi_array<uint8_t, 3> &base);
     * @brief Read an image from map_name and game_mode to boost::multi_array.
     * @param map_name The name of the map.
     * @param game_mode The game mode.
     * @param base Output variable to store the contents of the image.
     */
    void read_mini_map(const std::string &map_name, const std::string &game_mode, boost::multi_array<uint8_t, 3> &base);

    /**
     * @fn void read_png_ll(const std::string &in, boost::multi_array<uint8_t, 3> &image)
     * Reads RGB or RGBA image from string into an RGBA image
     * @param in Path of the image to be read
     * @param image The output variable, the array will be resized to contain the contents of 
     * the image.
     */
    void read_png_ll(const std::string &in, boost::multi_array<uint8_t, 3> &image);
}

#endif /* defined(wotreplay_image_h) */
