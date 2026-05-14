#include "animation_writer.h"
#include "fstream_ioctx.h"
#include "gd.h"
#include "gd_io.h"
#include "gdfontl.h"
#include "gdfontt.h"
#include "logger.h"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <numbers>
#include <string>

#include <boost/filesystem.hpp>

using namespace wotreplay;

const int TURRET_LINE_LENGTH = 10;

int animation_writer_t::update_model(const game_t &game, float window_start, float window_size, int packet_start) {
    int ix = packet_start;
    const auto &packets = game.get_packets();
    std::map<int, std::tuple<float, float, float>> tracks;
    std::map<int, float> turrets;
    std::map<int, float> hulls;

    float window_end = window_start + window_size;

    while (ix < packets.size() && (!packets[ix].has_property(property_t::clock) || packets[ix].clock() <= window_end)) {
        if (packets[ix].has_property(property_t::position)) {
            tracks[packets[ix].player_id()] = packets[ix].position();
            hulls[packets[ix].player_id()] = std::get<2>(packets[ix].rotation());
            this->packets[packets[ix].player_id()].emplace_back(packets[ix]);
        }

        if (packets[ix].has_property(property_t::turret_orientation)) {
            turrets[packets[ix].player_id()] = packets[ix].turret_orientation();
        }

        ix += 1;
    }

    for (auto &it : tracks) {
        this->tracks[it.first].emplace_back(it.second);
    }

    for (auto &it : turrets) {
        this->turrets[it.first].emplace_back(it.second);
    }

    for (auto &it : hulls) {
        this->hulls[it.first].emplace_back(it.second);
    }

    return ix;
}

gdImagePtr animation_writer_t::create_background_frame(const game_t &game) const {
    gdImagePtr result = gdImageCreateTrueColor(this->image_width, this->image_height);

    if (no_basemap) {
        int t = gdImageColorAllocate(result, 0x01, 0x01, 0x01);
        gdImageFill(result, 0, 0, t);
        gdImageColorTransparent(result, t);
    } else {
        auto shape = base.shape();
        for (int i = 0; i < shape[0]; i += 1) {
            for (int j = 0; j < shape[1]; j += 1) {
                int c = gdTrueColor(base[i][j][0], base[i][j][1], base[i][j][2]);
                gdImageSetPixel(result, j, i, c);
            }
        }
    }

    return result;
}

void animation_writer_t::set_max_history(int max_history) { this->max_history = max_history; }

void animation_writer_t::set_show_orientation(bool show_orientation) { this->show_orientation = show_orientation; }

gdImagePtr animation_writer_t::create_frame(const game_t &game, gdImagePtr background, float clock) const {
    gdImagePtr frame = gdImageCreateTrueColor(gdImageSX(background), gdImageSY(background));

    gdImageCopy(frame, background, 0, 0, 0, 0, gdImageSX(frame), gdImageSY(frame));

    int r = gdImageColorExact(frame, 0xFF, 0x00, 0x00);
    int g = gdImageColorExact(frame, 0x00, 0xFF, 0x00);
    int b = gdImageColorExact(frame, 0x00, 0x00, 0xFF);
    int w = gdImageColorExact(frame, 0xFF, 0xFF, 0xFF);
    int cyan = gdImageColorExact(frame, 0x00, 0xFF, 0xFF);

    gdImageString(frame, gdFontLarge, 10, 10, (uint8_t *)std::format("{}", clock).c_str(), cyan);

    int recorder_team = game.get_team_id(game.get_recorder_id());
    int recorder_id = game.get_recorder_id();

    for (auto &track : tracks) {
        const auto &positions = track.second;

        int player_team = game.get_team_id(track.first);
        if (player_team == -1) {
            continue;
        }

        int c;

        if (recorder_id == track.first) {
            c = b;
        } else if (player_team == -1) {
            c = w;
        } else if (player_team == recorder_team) {
            c = g;
        } else {
            c = r;
        }

        int history_pos = 0;
        for (auto it = positions.rbegin(); it != positions.rend(); it++) {
            if (max_history != -1 && history_pos >= max_history) {
                break;
            }

            auto [x, y] = get_2d_coord(*it, this->arena.bounding_box, this->image_width, this->image_height);
            gdImageSetPixel(frame, x, y, c);

            history_pos += 1;
        }

        auto [x, y] = get_2d_coord(positions.back(), this->arena.bounding_box, this->image_width, this->image_height);
        gdImageFilledRectangle(frame, x - 1, y - 1, x + 1, y + 1, c);

        const auto player_display_name = game.get_player(track.first).name;
        gdImageString(frame, gdFontTiny, x - 50, y, (uint8_t *)player_display_name.c_str(), c);

        if (show_orientation && packets.contains(track.first)) {
            gdImageLine(frame, x, y, x + TURRET_LINE_LENGTH * std::cos(packets.at(track.first).back().get_data_field<float>(48) - std::numbers::pi / 2),
                        y + TURRET_LINE_LENGTH * std::sin(packets.at(track.first).back().get_data_field<float>(48) - std::numbers::pi / 2), cyan);
        }

        if (show_turrets && turrets.contains(track.first)) {
            gdImageLine(frame, x, y, x + TURRET_LINE_LENGTH * std::cos(turrets.at(track.first).back() - std::numbers::pi / 2),
                        y + TURRET_LINE_LENGTH * std::sin(turrets.at(track.first).back() - std::numbers::pi / 2), w);
        }
    }

    if (no_basemap) {
        gdImageColorTransparent(frame, gdImageGetTransparent(background));
    }

    return frame;
}

void animation_writer_t::write(std::ostream &os) {
    int size;
    void *data = gdDPExtractData(ctx, &size);
    os.write((const char *)data, size);
}

void animation_writer_t::set_model_update_rate(int model_update_rate) { this->model_update_rate = model_update_rate; }

void animation_writer_t::set_frame_rate(int frame_rate) { this->frame_rate = frame_rate; }

void animation_writer_t::set_show_turrets(bool show_turrets) { this->show_turrets = show_turrets; }

void animation_writer_t::update(const game_t &game) {
    draw_basemap();

    gdImagePtr previous = NULL, background = create_background_frame(game), frame = create_frame(game, background, 0.f);

    float window_start = 0.f;

    gdImageGifAnimBeginCtx(background, ctx, 1, 0);

    const auto &packets = game.get_packets();

    int ix = 0, total_packets = packets.size();

    int frame_count = 0;

    float df = 1.f / frame_rate;
    float dm = 1.f / model_update_rate;

    int frame_nr = 0;
    while (ix < total_packets) {
        frame_nr += 1;

        for (float ds = 0; ds < df && ix < total_packets; ds += dm) {
            window_start = packets[ix].clock();
            ix = this->update_model(game, window_start, dm, ix);
        }

        frame = create_frame(game, background, window_start);

        logger.writef(log_level_t::debug, "generating gif frame frame_nr=%1%\n", frame_nr);

        if (!raw_images_path.empty()) {
            const auto file_name = std::format("{}/{:010}.png", raw_images_path, frame_nr);
            std::ofstream of(file_name, std::ios::binary | std::ios::out);
            OfstreamIOCtx ctx(of);
            gdImagePngCtx(frame, (gdIOCtxPtr)&ctx);
        }

        gdImageTrueColorToPalette(frame, 1, 255);
        gdImageGifAnimAddCtx(frame, ctx, 1, 0, 0, (int)(100 * df), gdDisposalNone, previous);

        if (previous) {
            gdImageDestroy(previous);
        }

        previous = frame;
    }

    if (frame) {
        gdImageDestroy(frame);
    }

    gdImageDestroy(background);

    gdImageGifAnimEndCtx(ctx);
}

void animation_writer_t::set_raw_images_path(const std::string &raw_images_path) { this->raw_images_path = raw_images_path; }

void animation_writer_t::init(const arena_t &arena, const std::string &mode) {
    image_writer_t::init(arena, mode);

    ctx = gdNewDynamicCtx(100 * 1024 * 1024, NULL);

    if (!raw_images_path.empty() && !boost::filesystem::exists(raw_images_path)) {
        logger.writef("create raw images directory: %1%\n", raw_images_path);
        boost::filesystem::create_directory(raw_images_path);
    }
}

void animation_writer_t::finish() {}

animation_writer_t::~animation_writer_t() { ctx->gd_free(ctx); }
