#include "animation_writer.h"
#include "logger.h"

using namespace wotreplay;

int animation_writer_t::update_model(const game_t &game, float window_start, float window_size, int packet_start) {
    int ix = packet_start;
    const auto &packets = game.get_packets();
    std::map<int, std::tuple<float, float, float>> positions;

    float window_end = window_start + window_size;

    while (ix < packets.size() && packets[ix].clock() <= window_end) {
        if (packets[ix].has_property(property_t::position)) {
            positions[packets[ix].player_id()] = packets[ix].position();
        }

        ix += 1;
    }

    for (auto &it = positions.begin(); it != positions.end(); it++) {
        if (game.get_team_id(it->first) != -1) {
            tracks[it->first].emplace_back(it->second);
        }
    }

    return ix;
}

gdImagePtr animation_writer_t::create_frame(const game_t &game, bool background) const {
    gdImagePtr frame = gdImageCreateTrueColor(this->image_width, this->image_height);

    auto shape = base.shape();
    for (int i = 0; i < shape[0]; i += 1) {
        for (int j = 0; j < shape[1]; j += 1) {
            int c = gdTrueColor(base[i][j][0], base[i][j][1], base[i][j][2]);
            gdImageSetPixel(frame, j, i, c);
        }
    }

    int r = gdTrueColor(0xFF, 0x00, 0x00);
    int g = gdTrueColor(0x00, 0xFF, 0x00);
    int recorder_team = game.get_team_id(game.get_recorder_id());

    for (auto &track = tracks.begin(); track != tracks.end(); track++) {
        const auto &positions = track->second;
        int player_team = game.get_team_id(track->first);
        int c = player_team == recorder_team ? g : r;

        for (auto &pos = positions.begin(); pos != positions.end(); pos++) {
            auto xy_pos = get_2d_coord(*pos, this->arena.bounding_box, this->image_width, this->image_height);
            gdImageSetPixel(frame, std::get<1>(xy_pos), std::get<0>(xy_pos), c);
        }
    }

    return frame;
}

void animation_writer_t::write(std::ostream &os) {
    int size;
    void *data = gdDPExtractData(ctx, &size);
    os.write((const char*) data, size);
}

void animation_writer_t::update(const game_t & game)
{
    set_no_basemap(false);
    draw_basemap();

    gdImagePtr previous = NULL,
        frame = create_frame(game, true);

    float window_start = 0.f,
        window_size = 0.1f,
        rate = 1.f;

    gdImageGifAnimBeginCtx(frame, ctx, 1, 0);

    gdImageGifAnimAddCtx(frame, ctx, 0, 0, 0, 10, 1, previous);

    previous = frame;

    const auto &packets = game.get_packets();

    int ix = 0,
        total_packets = packets.size();

    int frame_count = 0;

    while (ix < total_packets) {
        for (float ds = 0; ds < (1.f / rate); ds += window_size) {
            if (ix >= total_packets) break;
            window_start = packets[ix].clock();
            ix = this->update_model(game, window_start, window_size, ix);
        }

        frame = create_frame(game, false);

        gdImageGifAnimAddCtx(frame, ctx, 1, 0, 0, 10, gdDisposalRestoreBackground, previous);

        if (previous) gdImageDestroy(previous);

        previous = frame;
    }

    if (!frame) gdImageDestroy(frame);

    gdImageGifAnimEndCtx(ctx);
}

void animation_writer_t::init(const arena_t &arena, const std::string &mode)
{
    image_writer_t::init(arena, mode);

    ctx = gdNewDynamicCtxEx(2048, NULL, 1);
}

void animation_writer_t::finish() {

}

animation_writer_t::~animation_writer_t() {
    ctx->gd_free(ctx);
}