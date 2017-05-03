#ifndef wotreplay__class_heatmap_writer_h
#define wotreplay__class_heatmap_writer_h

#include "heatmap_writer.h"
#include "rule.h"

namespace wotreplay {

    /**
     * wotreplay::class_heatmap_writer_t draws a heatmap using draw rules
     */
    class class_heatmap_writer_t : public heatmap_writer_t {
    public:
        virtual void init(const arena_t &arena, const std::string &mode) override;
        /**
         * Define drawing rules to use for generating the heat map
         * @param rules set new drawing rules
         */
        virtual void set_draw_rules(const std::vector<draw_rule_t> &rules);
        /**
         * Get the drawing rules
         * @return current drawing rules
         */
        virtual const std::vector<draw_rule_t> &get_draw_rules() const;
        virtual int get_class(const game_t &game, const packet_t &packet) const override;
        virtual void finish() override;
    protected:
        std::vector<draw_rule_t> rules;
        std::map<uint32_t, int> classes;
    };
}

#endif
