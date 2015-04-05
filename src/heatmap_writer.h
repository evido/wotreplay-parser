#ifndef wotreplay__heatmap_writer_h
#define wotreplay__heatmap_writer_h

#include "image_writer.h"

/** @file */

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
        virtual int  get_class(const game_t &game, const packet_t &packet) const;
        virtual void finish() override;
        /** Skip number of seconds after start of battle */
        float skip;
        /** Modify boundaries for heatmap colors */
        std::tuple<double, double> bounds;
        /** Heatmap mode */
        heatmap_mode_t mode;
    };

    /**
     * @fn std::tuple<float, float> get_bounds(boost::multi_array<float, 3>::const_reference image, float l_quant,float r_quant)
     * Determine nth_value from sorted arrays for lower and upper bound
     * @param image image  with values
     * @param l_quant lower bound to find
     * @param r_quant upper bound to find
     * @return bounds values
     */
    std::tuple<float, float> get_bounds(boost::multi_array<float, 3>::const_reference image,
                                        float l_quant, float r_quant);
}

#endif
