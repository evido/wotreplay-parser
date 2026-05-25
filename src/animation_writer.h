#pragma once
#include "packet.h"
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
    gdImagePtr create_frame(const game_t &game, gdImagePtr background, float clock) const;
    gdImagePtr create_background_frame(const game_t &game) const;
    virtual void set_frame_rate(int frame_rate);
    virtual void set_model_update_rate(int model_update_rate);
    void set_max_history(int max_history);
    void set_raw_images_path(const std::string &raw_images_path);
    void set_show_turrets(bool show_turrets);
    void set_show_orientation(bool show_orientation);
    void set_skip(double skip);

  private:
    gdIOCtx *ctx;
    std::map<int, std::deque<std::tuple<float, float, float>>> tracks;
    std::map<int, std::deque<float>> turrets;
    std::map<int, std::deque<float>> hulls;
    std::map<int, std::deque<packet_t>> packets;
    std::map<int, int> current_health;
    std::map<int, int> max_health;
    int frame_rate, model_update_rate;
    int max_history;
    std::string raw_images_path;
    bool show_turrets;
    bool show_orientation;
    double skip;
};
} // namespace wotreplay

#endif
