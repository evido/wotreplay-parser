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

#include "game.h"
#include "image.h"

#include <stdexcept>

using namespace wotreplay;

bool wotreplay::write_png(const std::string &out, png_bytepp image, size_t width, size_t height, bool alpha) {
    FILE *fp = fopen(out.c_str(), "wb");

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_filter(png_ptr, 0,PNG_FILTER_VALUE_NONE);
    png_set_IHDR(png_ptr, info_ptr, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                 8, alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_rows(png_ptr, info_ptr, image);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return true;
}

void wotreplay::read_png(const std::string &in, png_bytepp image, int &width, int &height, int &channels) {
    FILE *fp = fopen(in.c_str(), "rb");
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_infop end_info = png_create_info_struct(png_ptr);
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return;
    }
    png_init_io(png_ptr, fp);
    png_set_rows(png_ptr, info_ptr, image);
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);
    height = png_get_image_height(png_ptr, info_ptr);
    width = png_get_image_width(png_ptr, info_ptr);
    channels = png_get_channels(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(fp);
}

void wotreplay::get_row_pointers(boost::multi_array<uint8_t, 3> &image, std::vector<png_bytep> &row_pointers) {
    const size_t *shape = image.shape();
    size_t rows = shape[0], column_bytes = shape[1]*shape[2];
    for (int i = 0; i < rows; ++i) {
        row_pointers.push_back(image.data() + i*column_bytes);
    }
}

void wotreplay::write_image(const std::string &file_name, boost::multi_array<uint8_t, 3> &image) {
    const size_t *shape = image.shape();
    std::vector<png_bytep> row_pointers;
    get_row_pointers(image, row_pointers);
    write_png(file_name, &row_pointers[0], shape[0], shape[1], shape[2] == 4);
}

void wotreplay::read_mini_map(const std::string &map_name, const std::string &game_mode, boost::multi_array<uint8_t, 3> &base) {
    base.resize(boost::extents[500][500][4]);
    std::vector<png_bytep> row_pointers;
    get_row_pointers(base, row_pointers);
    int width, height, channels;
    std::string mini_map = "maps/no-border/" + map_name + "_" + game_mode + ".png";
    read_png(mini_map, &row_pointers[0], width, height, channels);
}

image_t::image_t(const std::string &map_name, const std::string &game_mode)
{
    // base map determines the size of the 'images'
    load_base_map(map_name, game_mode);
    const size_t *shape = base.shape();
    size_t height = shape[0], width = shape[1];
    positions.resize(boost::extents[3][height][width]);
    deaths.resize(boost::extents[3][height][width]);
}

void image_t::draw_death(const packet_t &packet, const game_t &game) {
    uint32_t killer, killed;
    std::tie(killed, killer) = packet.tank_destroyed();
    packet_t position_packet;
    bool found = game.find_property(packet.clock(), killed, property_t::position, position_packet);
    if (found) {
        draw_position(position_packet, game, this->deaths);
    }
}

void image_t::draw_position(const packet_t &packet, const game_t &game, boost::multi_array<float, 3> &image) {
    uint32_t player_id = packet.player_id();

    int team_id = game.get_team_id(player_id);
    if (team_id < 0) return;

    auto shape = image.shape();
    int width = static_cast<int>(shape[2]);
    int height = static_cast<int>(shape[1]);

    float x,y;
    std::tie(x,y) = get_2d_coord( packet.position(), game, width, height);

    if (x >= 0 && y >= 0 && x <= (width - 1) && y <= (height - 1)) {
        image[team_id][y][x] = 1;

        if (player_id == game.get_recorder_id()) {
            image[2][y][x] = 1;
        }
    }
}

void image_t::update(const game_t &game) {
    for (const packet_t &packet : game.get_packets()) {
        if (packet.has_property(property_t::position)
                && dead_players.find(packet.player_id()) == dead_players.end()) {
            draw_position(packet, game, this->positions);
        } else if (packet.has_property(property_t::tank_destroyed)) {
            uint32_t target, killer;
            std::tie(target, killer) = packet.tank_destroyed();
            dead_players.insert(target);
            draw_death(packet, game);
        }
    }
}

void image_t::load_base_map(const std::string &map_name, const std::string &game_mode) {
    std::string mini_map = "maps/no-border/" + map_name + "_" + game_mode + ".png";
    read_png_ll(mini_map,this->base);
}

void wotreplay::read_png_ll(const std::string &in, boost::multi_array<uint8_t, 3> &image) {
    FILE *fp = fopen(in.c_str(), "rb");
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_infop end_info = png_create_info_struct(png_ptr);
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return;
    }
    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);
    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    if (color_type == PNG_COLOR_TYPE_RGB) {
        // add alpha channel
        png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
    } else if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
        // do nothing extra
    } else {
        throw std::runtime_error("Unsupported image type");
    }

    image.resize(boost::extents[height][width][4]);
    std::vector<png_bytep> row_pointers;
    get_row_pointers(image, row_pointers);
    png_read_image(png_ptr, &row_pointers[0]);
    png_read_end(png_ptr, info_ptr);
    fclose(fp);
}

void image_t::write_image(const std::string &path) {
    wotreplay::write_image(path, result);
}

void image_t::finish() {
    // copy background to result
    const size_t *shape = base.shape();
    result.resize(boost::extents[shape[0]][shape[1]][shape[2]]);
    result = base;
    
    for (int i = 0; i < shape[0]; ++i) {
        for (int j = 0; j < shape[1]; ++j) {
            if (positions[0][i][j] > positions[1][i][j]) {
                // position claimed by first team
                result[i][j][0] = result[i][j][2] = 0x00;
                result[i][j][1] = result[i][j][3] = 0xFF;
            } else if (positions[0][i][j] < positions[1][i][j]) {
                // position claimed by second team
                result[i][j][1] = result[i][j][2] = 0x00;
                result[i][j][0] = result[i][j][3] = 0xFF;
            } else {
                // no change
            }

            if (positions[2][i][j] > 0.f) {
                // position claimed by second team
                result[i][j][0] = result[i][j][1] = 0x00;
                result[i][j][2] = result[i][j][3] = 0xFF;
            }


            if (deaths[0][i][j] + deaths[1][i][j] > 0.f) {
                static int offsets[][2] = {
                    {-1, -1},
                    {-1,  0},
                    {-1,  1},
                    { 0,  1},
                    { 1,  1},
                    { 1,  0},
                    { 1, -1},
                    { 0, -1},
                    { 0,  0}
                };

            
                for (auto offset : offsets) {
                    int x = j + offset[0];
                    int y = i + offset[1];
                    result[y][x][3] = result[y][x][0] = result[y][x][1] = 0xFF;                    
                    result[y][x][2] = 0x00;
                }
            }
        }
    }
}

int wotreplay::mix(int v0, int v1, float a1, int v2, float a2) {
    return (1-a2)*((1-a1)*v0 + a1*v1) + a2 * v2;
}

void image_t::reset() {
    std::fill(positions.origin(), positions.origin() + deaths.num_elements(), 0.f);
    std::fill(positions.origin(), positions.origin() + positions.num_elements(), 0.f);
}
