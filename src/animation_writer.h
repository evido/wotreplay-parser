#pragma once
#ifndef wotreplay_animation_writer_h
#define wotreplay_animation_writer_h

#include "image_writer.h"

#include <deque>
#include <gd.h>

namespace wotreplay {
    class animation_writer_t : public image_writer_t {
    public:
        virtual void write(std::ostream &os);
        virtual void update(const game_t &game);
        virtual void finish();
        virtual void init(const arena_t &arena, const std::string &mode);
        virtual ~animation_writer_t();
        int update_model(const game_t &game, float window_start, float window_size, int packet_start);
        gdImagePtr create_frame(const game_t &game, bool background) const;
    private:
        gdIOCtx *ctx;
        std::map<int, std::deque<std::tuple<float, float, float>>> tracks;
    };
}

#endif