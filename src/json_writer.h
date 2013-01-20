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
        virtual void update(const game_t &game) override;
        virtual void finish() override;
        virtual void reset() override;
        virtual void write(std::ostream &os) override;
        virtual bool is_initialized() const override;
        virtual void init(const std::string &map, const std::string &mode) override;
        virtual void clear() override;
    private:
        Json::Value value;
        bool initialized;
    };
}

#endif // wotreplay__json_writer_h

