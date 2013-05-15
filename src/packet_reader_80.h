#ifndef wotreplay_packet_reader_80_h
#define wotreplay_packet_reader_80_h

#include "game.h"
#include "packet_reader.h"

#include <map>

namespace wotreplay {
    struct packet_size_t {
        int size;
        int payload_size_offset;
        int payload_size_type;
    };

    class packet_reader_80_t : public packet_reader_t {
    public:
        packet_reader_80_t(const version_t &version, buffer_t &buffer);
        virtual packet_t next();
        virtual bool has_next();
    private:
        std::map<uint8_t, packet_size_t> packet_sizes;
        buffer_t &buffer;
        version_t version;
        int pos;
    };
}

#endif /* defined(wotreplay_packet_reader_80_h) */
