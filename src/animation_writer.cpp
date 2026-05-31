#include "animation_writer.h"
#include "fstream_ioctx.h"
#include "game.h"
#include "gd.h"
#include "gd_io.h"
#include "gdfontl.h"
#include "gdfontmb.h"
#include "gdfonts.h"
#include "gdfontt.h"
#include "logger.h"
#include "packet.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <numbers>
#include <optional>
#include <ranges>
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

    std::erase_if(hits, [=](const packet_t &p) { return p.clock() + 1 <= window_start; });

    while (ix < packets.size() && (!packets[ix].has_property(property_t::clock) || packets[ix].clock() <= window_end)) {
        if (packets[ix].has_property(property_t::position)) {
            tracks[packets[ix].player_id()] = packets[ix].position();
            hulls[packets[ix].player_id()] = std::get<2>(packets[ix].rotation());
            this->packets[packets[ix].player_id()].emplace_back(packets[ix]);
        }

        if (packets[ix].has_property(property_t::turret_orientation)) {
            turrets[packets[ix].player_id()] = packets[ix].turret_orientation();
        }

        if (packets[ix].has_property(property_t::health)) {
            current_health[packets[ix].player_id()] = packets[ix].health();
        }

        if (packets[ix].has_property(property_t::max_health)) {
            max_health[packets[ix].player_id()] = packets[ix].max_health();
        }

        if (packets[ix].type() == 0x08 && packets[ix].sub_type() == 0x01) {
            hits.emplace_back(packets[ix]);
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

std::optional<packet_t> find_recent_position(const std::map<int, std::deque<packet_t>> &packets, int player_id, float clock) {
    if (!packets.contains(player_id)) {
        return std::nullopt;
    }

    const auto &player_packets = packets.at(player_id);

    const auto result = std::find_if(player_packets.rbegin(), player_packets.rend(), [=](const packet_t &p) { return std::abs(p.clock() - clock) < 1; });

    if (result == player_packets.rend()) {
        return std::nullopt;
    }

    return {*result};
}

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

    float f = image_width / (float)512;

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

        gdImageAlphaBlending(frame, gdEffectAlphaBlend);
        int history_pos = 0;
        for (auto it = positions.rbegin(); it != positions.rend(); it++) {
            if (max_history != -1 && history_pos >= max_history) {
                break;
            }

            auto [x, y] = get_2d_coord(*it, this->arena.bounding_box, this->image_width, this->image_height);

            float p = ((float)(history_pos) / (float)max_history);
            int blend = gdImageColorAllocateAlpha(frame, gdTrueColorGetRed(c), gdTrueColorGetGreen(c), gdTrueColorGetBlue(c), 128 * (p * p * p));

            gdImageSetPixel(frame, x, y, blend);

            history_pos += 1;
        }
        gdImageAlphaBlending(frame, gdEffectReplace);

        auto [x, y] = get_2d_coord(positions.back(), this->arena.bounding_box, this->image_width, this->image_height);
        gdImageFilledRectangle(frame, x - f * 1, y - f * 1, x + f * 1, y + f * 1, c);

        const auto player_display_name = game.get_player(track.first).name;

        gdFontPtr nameFont;

        if (image_width >= 1024) {
            nameFont = gdFontMediumBold;
        } else if (image_height >= 512) {
            nameFont = gdFontSmall;
        } else {
            nameFont = gdFontTiny;
        }

        int char_size = 6;
        int left_offset = x - 10 - player_display_name.length() * char_size;

        gdImageString(frame, nameFont, left_offset, y, (uint8_t *)player_display_name.c_str(), c);

        if (current_health.contains(track.first) && max_health.contains(track.first)) {
            float f = ((float)current_health.at(track.first)) / ((float)max_health.at(track.first));
            gdImageFilledRectangle(frame, x - 42, y - 3, x - 12, y + 0, r);

            if (std::find_if(hits.begin(), hits.end(), [&](const packet_t &p) { return p.player_id() == track.first; }) != hits.end()) {
                gdImageFilledRectangle(frame, x - 42, y - 3, x - 42 + 30 * f, y + 0, gdTrueColor(0xFF, 0xFF, 0x00));
            } else if (f > 0) {
                gdImageFilledRectangle(frame, x - 42, y - 3, x - 42 + 30 * f, y + 0, g);
            }
        } else {
            gdImageFilledRectangle(frame, x - 42, y - 3, x - 12, y + 0, w);
        }

        if (show_orientation && packets.contains(track.first)) {
            const auto o = packets.at(track.first).back().hull_orientation2();

            if (player_team == recorder_team) {
                gdImageLine(frame, x, y, x + f * TURRET_LINE_LENGTH * std::cos(o - std::numbers::pi / 2),
                            y + f * TURRET_LINE_LENGTH * std::sin(o - std::numbers::pi / 2), cyan);
            } else {
                gdImageLine(frame, x, y, x + f * TURRET_LINE_LENGTH * std::cos(o - std::numbers::pi / 2),
                            y + f * TURRET_LINE_LENGTH * std::sin(o - std::numbers::pi / 2), cyan);
            }
        }

        if (show_turrets && turrets.contains(track.first)) {
            const auto t = turrets.at(track.first).back() + packets.at(track.first).back().hull_orientation2();

            const std::array<std::tuple<float, int>, 4> turret_lines = {
                std::make_tuple(0.0f, r),
                std::make_tuple(1.0f, w),
                std::make_tuple(2.0f, g),
                std::make_tuple(3.0f, b),
            };

            for (const auto [r, c] : turret_lines) {
                if (debug || c == w) {
                    gdImageLine(frame, x, y, std::round(x + f * TURRET_LINE_LENGTH * std::cos(t + r * std::numbers::pi / 2)),
                                std::round(y + f * TURRET_LINE_LENGTH * std::sin(t + r * std::numbers::pi / 2)), c);
                }
            }
        }

        for (const auto &hit : hits) {
            const auto &player_position = find_recent_position(packets, hit.player_id(), hit.clock());

            if (!player_position.has_value()) {
                logger.writef(log_level_t::warning, "[animation_writer] unable to locate player_id=%1% data=%2%\n", hit.player_id(), hit);
                continue;
            }

            const auto &source_position = find_recent_position(packets, hit.source(), hit.clock());
            if (!source_position.has_value()) {
                logger.writef(log_level_t::warning, "[animation_writer] unable to locate source=%1% data=%2%\n", hit.source(), hit);
                continue;
            }

            auto [target_x, target_y] = get_2d_coord(player_position->position(), this->arena.bounding_box, this->image_width, this->image_height);

            auto [source_x, source_y] = get_2d_coord(source_position->position(), this->arena.bounding_box, this->image_width, this->image_height);
            gdImageLine(frame, target_x, target_y, source_x, source_y, gdTrueColor(0xFF, 0xFF, 0x00));
        }
    }

    if (no_basemap) {
        gdImageColorTransparent(frame, gdImageGetTransparent(background));
    }

    if (debug) {
        static int debug_frame_nr = 0;
        static std::set<int> rendered_hits;

        for (const auto &hit : hits) {
            const int hit_id = (int)hit.clock() * 1000;
            if (rendered_hits.contains(hit_id)) {
                continue;
            }

            gdImagePtr debug_frame = gdImageCreateTrueColor(gdImageSX(background), gdImageSY(background));
            gdImageCopy(debug_frame, background, 0, 0, 0, 0, gdImageSX(debug_frame), gdImageSY(debug_frame));

            gdImageString(debug_frame, gdFontLarge, 10, 10, (uint8_t *)std::format("[{}] {}", debug_frame_nr, clock).c_str(), cyan);

            const auto &player_position = find_recent_position(packets, hit.player_id(), hit.clock());

            if (!player_position.has_value()) {
                logger.writef(log_level_t::warning, "[animation_writer] unable to locate player_id=%1% data=%2%\n", hit.player_id(), hit);
                continue;
            }

            const auto &source_position = find_recent_position(packets, hit.source(), hit.clock());
            if (!source_position.has_value()) {
                logger.writef(log_level_t::warning, "[animation_writer] unable to locate source=%1% data=%2%\n", hit.source(), hit);
                continue;
            }

            auto filtered = game.get_packets() | std::views::filter([=](const packet_t &p) { return p.has_property(property_t::turret_orientation); }) |
                            std::views::filter([=](const packet_t &p) { return p.player_id() == hit.source(); }) | std::views::common;

            auto turret = std::min_element(filtered.begin(), filtered.end(), [=](const packet_t &left, const packet_t &right) {
                return std::abs(left.clock() - hit.clock()) < std::abs(right.clock() - hit.clock());
            });

            auto [target_x, target_y] = get_2d_coord(player_position->position(), this->arena.bounding_box, this->image_width, this->image_height);

            auto [source_x, source_y] = get_2d_coord(source_position->position(), this->arena.bounding_box, this->image_width, this->image_height);
            gdImageLine(debug_frame, target_x, target_y, source_x, source_y, gdTrueColor(0xFF, 0xFF, 0x00));

            const auto o = packets.at(hit.source()).back().hull_orientation2();
            gdImageLine(debug_frame, source_x, source_y, source_x + f * TURRET_LINE_LENGTH * std::cos(o - std::numbers::pi / 2),
                        source_y + f * TURRET_LINE_LENGTH * std::sin(o - std::numbers::pi / 2), cyan);

            const auto t = turrets.at(hit.source()).back() + packets.at(hit.source()).back().hull_orientation2();

            const std::array<std::tuple<float, int>, 4> turret_lines = {
                std::make_tuple(1.0f, r),
                std::make_tuple(0.0f, w),
                std::make_tuple(2.0f, g),
                std::make_tuple(3.0f, b),
            };

            for (const auto [r, c] : turret_lines) {
                gdImageLine(debug_frame, source_x, source_y, std::round(source_x + f * TURRET_LINE_LENGTH * std::cos(t + r * std::numbers::pi / 2)),
                            std::round(source_y + f * TURRET_LINE_LENGTH * std::sin(t + r * std::numbers::pi / 2)), c);
            }

            const auto file_name = std::format("debug_frame_{:010}.png", debug_frame_nr);
            std::ofstream of(file_name, std::ios::binary | std::ios::out);
            OfstreamIOCtx ctx(of);
            gdImagePngCtx(debug_frame, (gdIOCtxPtr)&ctx);

            debug_frame_nr += 1;
            rendered_hits.emplace(hit_id);
            gdImageDestroy(debug_frame);
        }
    }

    return frame;
}

void animation_writer_t::write(std::ostream &os) {
    int size;
    void *data = gdDPExtractData(ctx, &size);
    os.write((const char *)data, size);
    os.flush();
}

void animation_writer_t::set_model_update_rate(int model_update_rate) { this->model_update_rate = model_update_rate; }

void animation_writer_t::set_frame_rate(int frame_rate) { this->frame_rate = frame_rate; }

void animation_writer_t::set_show_turrets(bool show_turrets) { this->show_turrets = show_turrets; }

void animation_writer_t::set_skip(double skip) { this->skip = skip; }

void animation_writer_t::set_debug(bool debug) { this->debug = debug; }

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
    int rendered_frame_nr = 0;
    while (ix < total_packets) {
        frame_nr += 1;

        for (float ds = 0; ds < df && ix < total_packets; ds += dm) {
            window_start = packets[ix].clock();
            ix = this->update_model(game, window_start, dm, ix);
        }

        if (window_start < skip) {
            continue;
        }

        rendered_frame_nr += 1;
        frame = create_frame(game, background, window_start);

        logger.writef(log_level_t::info, "generating gif frame frame_nr=%1% rendered_frame_nr=%2% window_start=%3%\n", frame_nr, rendered_frame_nr,
                      window_start);

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
        logger.writef(log_level_t::info, "create raw images directory: %1%\n", raw_images_path);
        boost::filesystem::create_directory(raw_images_path);
    }
}

void animation_writer_t::finish() {}

animation_writer_t::~animation_writer_t() { ctx->gd_free(ctx); }
