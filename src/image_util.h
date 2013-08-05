#ifndef wotreplay__image_util
#define wotreplay__image_util

#include <boost/multi_array.hpp>
#include <png.h>
#include <string>
#include <vector>

/** @file */

namespace wotreplay {
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
     * @fn void read_png(std::istream &is, boost::multi_array<uint8_t, 3> &image)
     * Reads RGB or RGBA image from string into an RGBA image
     * @param is inputstream to read the image from.
     * @param image The output variable, the array will be resized to contain the contents of
     * the image.
     */
    void read_png(std::istream &is, boost::multi_array<uint8_t, 3> &image);

    /**
     * @fn bool write_png(std::ostream &os, boost::multi_array<uint8_t, 3> &image)
     * @param os The outputstream to write the png image to.
     * @param image The image.
     */
    bool write_png(std::ostream &os, boost::multi_array<uint8_t, 3> &image);

    /**
     * Implementation of bilinear interpolation to resize an image
     * @param orignal original image
     * @param width the width of the new image
     * @param height the height of the new image
     * @param result the new image
     */
    void resize(boost::multi_array<uint8_t, 3> &original, int width, int height, boost::multi_array<uint8_t, 3> &result);
}

#endif /* defined(wotreplay__image_util) */
