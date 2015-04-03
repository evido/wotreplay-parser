#include "image_util.h"
#include "logger.h"

#include <boost/format.hpp>
#include <cmath>
#include <fstream>

using namespace wotreplay;

static_assert(sizeof(png_byte) <= sizeof(uint8_t), "Unexpected definition of png_byte");

static void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    void* io_ptr = png_get_io_ptr(png_ptr);
    std::istream *is = reinterpret_cast<std::istream*>(io_ptr);
    is->read(reinterpret_cast<char*>(data), length);
}

static void user_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    void* io_ptr = png_get_io_ptr(png_ptr);
    std::ostream *os = reinterpret_cast<std::ostream*>(io_ptr);
    os->write(reinterpret_cast<char*>(data), length);
}

static void user_flush_data(png_structp png_ptr) {
    void* io_ptr = png_get_io_ptr(png_ptr);
    std::ostream *os = reinterpret_cast<std::ostream*>(io_ptr);
    os->flush();
}

void wotreplay::read_png(std::istream &is, boost::multi_array<uint8_t, 3> &image) {
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_infop end_info = png_create_info_struct(png_ptr);
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        return;
    }
    
    png_set_read_fn(png_ptr, &is, &user_read_data);

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
}

bool wotreplay::write_png(std::ostream &os, boost::multi_array<uint8_t, 3> &image) {
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }

    png_set_write_fn(png_ptr, &os, &user_write_data, &user_flush_data);
    png_set_filter(png_ptr, 0,PNG_FILTER_VALUE_NONE);

    const size_t *shape = image.shape();
    size_t width = shape[1], height = shape[0], channels = shape[2];
    bool alpha = channels == 4;

    png_set_IHDR(png_ptr, info_ptr, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                 8, alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    std::vector<png_bytep> row_pointers;
    get_row_pointers(image, row_pointers);
    png_set_rows(png_ptr, info_ptr, &row_pointers[0]);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    
    return true;
}

void wotreplay::get_row_pointers(boost::multi_array<uint8_t, 3> &image, std::vector<png_bytep> &row_pointers) {
    const size_t *shape = image.shape();
    size_t rows = shape[0], column_bytes = shape[1]*shape[2];
    for (int i = 0; i < rows; ++i) {
        row_pointers.push_back(image.data() + i*column_bytes);
    }
}

void wotreplay::read_mini_map(const std::string &map_name, const std::string &game_mode, boost::multi_array<uint8_t, 3> &base) {
    std::string mini_map = (boost::format("maps/no-border/%1%_%2%.png") % map_name % game_mode).str();
    std::ifstream file(mini_map, std::ios::binary);
    read_png(file, base);
}

int wotreplay::mix(int v0, int v1, float a1, int v2, float a2) {
    return (1-a2)*((1-a1)*v0 + a1*v1) + a2 * v2;
}

void wotreplay::resize(boost::multi_array<uint8_t, 3> &original, int width, int height, boost::multi_array<uint8_t, 3> &result) {
    const size_t *shape = original.shape();
    result.resize(boost::extents[height][width][shape[2]]);

    float ty = ((float) shape[0] / height);
    float tx = ((float) shape[1] / width);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int xx = (int) (tx * x);
            int yy = (int) (ty * y);
            float dx = (tx * x) - xx;
            float dy = (ty * y) - yy;
            
            for (int c = 0; c < shape[2]; ++c) {
                result[y][x][c] = original[yy][xx][c]*(1-dy)*(1-dx) +
                                    original[std::min(yy + 1, (int) shape[0] - 1)][xx][c]*dy*(1-dx) +
                                    original[std::min(yy + 1, (int) shape[0] - 1)][std::min(xx + 1, (int) shape[1] - 1)][c]*dy*dx +
                                    original[yy][std::min(xx + 1, (int) shape[1]  - 1)][c]*(1-dy)*dx;
            }
        }
    }
}
