#include "image_writer.h"

namespace wotreplay {
    class heatmap_writer_t : public image_writer_t {
    public:
        heatmap_writer_t();
        virtual void update(const game_t &game) override;
        virtual void finish();
        float skip;
        std::tuple<double, double> bounds;
    };
}
