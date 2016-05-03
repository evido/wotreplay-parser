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

    for (auto &it: positions) {
        if (game.get_team_id(it.first) != -1) {
            tracks[it.first].emplace_back(it.second);
        }
    }

    return ix;
}

gdImagePtr animation_writer_t::create_background_frame(const game_t &game) const {
    gdImagePtr result = NULL;

    if (no_basemap) {
        result = gdImageCreatePalette(this->image_width, this->image_height);
    }
    else {
        gdImagePtr frame = gdImageCreateTrueColor(this->image_width, this->image_height);

        auto shape = base.shape();
        for (int i = 0; i < shape[0]; i += 1) {
            for (int j = 0; j < shape[1]; j += 1) {
                int c = gdTrueColor(base[i][j][0], base[i][j][1], base[i][j][2]);
                gdImageSetPixel(frame, j, i, c);
            }
        }

        result = gdImageCreatePaletteFromTrueColor(frame, 0, 150);
        gdImageDestroy(frame);        
    }

    int r = gdImageColorAllocate(result, 0xFF, 0x00, 0x00);
    int g = gdImageColorAllocate(result, 0x00, 0xFF, 0x00);    
    int b = gdImageColorAllocate(result, 0x00, 0x00, 0xFF);
    int t = gdImageColorAllocate(result, 0x01, 0x01, 0x01);

    if (no_basemap) gdImageFill(result, 0, 0, t);

    gdImageColorTransparent(result, t);

    return result;
}

gdImagePtr animation_writer_t::create_frame(const game_t &game, gdImagePtr background) const {
    gdImagePtr frame = gdImageCreate(this->image_width, this->image_height);

    gdImagePaletteCopy(frame, background);
    gdImageCopy(frame, background, 0, 0, 0, 0, this->image_width, this->image_height);
        
    int r = gdImageColorExact(frame, 0xFF, 0x00, 0x00);
    int g = gdImageColorExact(frame, 0x00, 0xFF, 0x00);        
    int b = gdImageColorExact(frame, 0x00, 0x00, 0xFF);

    int recorder_team = game.get_team_id(game.get_recorder_id());
    int recorder_id = game.get_recorder_id();

    for (auto &track : tracks) {
        const auto &positions = track.second;
        int player_team = game.get_team_id(track.first);
        int c = recorder_id == track.first ? b : (player_team == recorder_team ? g : r);

        for (auto &pos : positions) {
            auto xy_pos = get_2d_coord(pos, this->arena.bounding_box, this->image_width, this->image_height);
            gdImageSetPixel(frame, std::get<0>(xy_pos), std::get<1>(xy_pos), c);
        }
    }

    gdImageColorTransparent(frame, gdImageGetTransparent(background));

    return frame;
}

void animation_writer_t::write(std::ostream &os) {
    int size;
    void *data = gdDPExtractData(ctx, &size);
    os.write((const char*) data, size);
}

void animation_writer_t::set_model_update_rate(int model_update_rate) {
    this->model_update_rate = model_update_rate;
}

void animation_writer_t::set_frame_rate(int frame_rate) {
    this->frame_rate = frame_rate;
}

void animation_writer_t::update(const game_t & game)
{
    draw_basemap();

    gdImagePtr previous = NULL,
        background = create_background_frame(game),
        frame = create_frame(game, background);

    float window_start = 0.f;

    gdImageGifAnimBeginCtx(background, ctx, 1, 0);
    gdImageGifAnimAddCtx(background, ctx, 0, 0, 0, 10, gdDisposalNone, NULL);

    previous = frame;

    const auto &packets = game.get_packets();

    int ix = 0,
        total_packets = packets.size();

    int frame_count = 0;

    float df = 1.f / frame_rate;
    float dm = 1.f / model_update_rate;

    while (ix < total_packets) {
        for (float ds = 0; ds < df && ix < total_packets; ds += dm) {
            window_start = packets[ix].clock();
            ix = this->update_model(game, window_start, dm, ix);
        }

        frame = create_frame(game, background);

        gdImageGifAnimAddCtx(frame, ctx, 0, 0, 0, (int) 100 * df, gdDisposalNone, previous);

        if (previous) gdImageDestroy(previous);

        previous = frame;
    }

    if (!frame) gdImageDestroy(frame);

    gdImageDestroy(background);

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
