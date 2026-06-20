#pragma once
#include "packet.h"
#include <flat_map>
#include <vector>
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
    void set_use_player_health(bool use_player_health);
    void set_skip(double skip);
    void set_debug(bool debug);

  private:
    gdIOCtx *ctx;
    std::flat_map<int, std::vector<packet_t>> turrets;
    std::flat_map<int, std::vector<packet_t>> tracks;
    std::flat_map<int, packet_t> current_health;
    std::flat_map<int, packet_t> max_health;
    std::vector<packet_t> hits;
    std::vector<packet_t> hit_positions;
    int frame_rate, model_update_rate;
    int max_history;
    std::string raw_images_path;
    bool show_turrets;
    bool show_orientation;
    bool use_player_health;
    double skip;
    bool debug;
};
} // namespace wotreplay

#endif
