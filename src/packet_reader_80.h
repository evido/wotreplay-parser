#ifndef wotreplay_packet_reader_80_h
#define wotreplay_packet_reader_80_h

#include "packet_reader.h"

#include <map>

namespace wotreplay {
    /**
     * Contains parameter to read a packet
     */
    struct packet_config_t {
        /**
         * The base packet size
         */
        int size;
        /**
         * The position of the payload length indicator
         */
        int payload_length_offset;
    };

    /**
     * Read WOT 8.0 type replays
     */
    class packet_reader_80_t : public packet_reader_t {
    public:
        virtual void init(const version_t &version, buffer_t *buffer, game_title_t title);
        virtual packet_t next();
        virtual bool has_next();
        virtual bool is_compatible(const version_t &version);
    private:
        buffer_t *buffer;
        version_t version;
        int pos, prev;
		game_title_t title;
    };
}

#endif /* defined(wotreplay_packet_reader_80_h) */
