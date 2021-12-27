#ifndef wotreplay__json_writer_h
#define wotreplay__json_writer_h

#include "game.h"
#include "json/json.h"
#include "packet.h"
#include "writer.h"

namespace wotreplay {
    /**
     * Output WOT game as a json structure.
     */
    class json_writer_t : public writer_t {
    public:
        /** default constructor */
        json_writer_t();
        virtual void update(const game_t &game) override;
        virtual void finish() override;
        virtual void reset() override;
        virtual void write(std::ostream &os) override;
        virtual bool is_initialized() const override;
        virtual void init(const arena_t &arena, const std::string &mode) override;
        virtual void clear() override;
        virtual void set_filter(filter_t filter) override;
    private:
        Json::Value root;
        bool initialized;
        filter_t filter;
    };
}

#endif // wotreplay__json_writer_h
