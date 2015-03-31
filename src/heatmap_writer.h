#ifndef wotreplay__heatmap_writer_h
#define wotreplay__heatmap_writer_h

#include "image_writer.h"

namespace wotreplay {
    enum heatmap_mode_t {
        combined,
        team,
        team_soft
    };

    /**
     * wotreplay::heatmap_writer_t draws a heatmap from wotreplay::packet_t on a minimap
     */
    class heatmap_writer_t : public image_writer_t {
    public:
        heatmap_writer_t();
        virtual void update(const game_t &game) override;
        virtual void finish() override;
        /** Skip number of seconds after start of battle */
        float skip;
        /** Modify boundaries for heatmap colors */
        std::tuple<double, double> bounds;
        heatmap_mode_t mode;
    };

    std::tuple<float, float> get_bounds(boost::multi_array<float, 3>::const_reference image,
                                        float l_quant,float r_quant);
}

#endif
