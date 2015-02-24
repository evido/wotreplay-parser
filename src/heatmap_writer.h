#include "image_writer.h"

namespace wotreplay {
    /**
     * wotreplay::heatmap_writer_t draws a heatmap from wotreplay::packet_t on a minimap
     */
    class heatmap_writer_t : public image_writer_t {
    public:
        heatmap_writer_t();
        virtual void update(const game_t &game) override;
        virtual void finish();
        /** Skip number of seconds after start of battle */
        float skip;
        /** Modify boundaries for heatmap colors */
        std::tuple<double, double> bounds;
        bool combined;
    };
}
