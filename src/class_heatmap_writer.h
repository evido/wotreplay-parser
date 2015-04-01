#ifndef wotreplay__class_heatmap_writer_h
#define wotreplay__class_heatmap_writer_h

#include "heatmap_writer.h"
#include "rule.h"

namespace wotreplay {
    class class_heatmap_writer_t : public heatmap_writer_t {
    public:
        virtual void init(const arena_t &arena, const std::string &mode) override;
        virtual void set_draw_rules(const draw_rules_t &rules);
        virtual const draw_rules_t &get_draw_rules() const;
        virtual void update(const game_t &game) override;
        virtual void finish() override;
    protected:
        draw_rules_t rules;
        std::map<uint32_t, int> classes;
    };
}

#endif
