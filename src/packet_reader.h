#ifndef wotreplay_packet_reader_h
#define wotreplay_packet_reader_h

#include "packet.h"
#include "types.h"

namespace wotreplay {
    /**
     * Interface that provides a set of methodes to process a buffer.
     */
    class packet_reader_t {
    public:
        /**
         * Reads the next packet from the buffer, expects that there is a new packet
         * available.
         * @return the next packet in the buffer
         */
        virtual packet_t next() = 0;
        /**
         * Determines if there is another packet available in the buffer
         * @return \c true if there is a next packet available \c false if there is no such packet
         */
        virtual bool has_next() = 0;
        virtual ~packet_reader_t() {}
    };
}

#endif /* defined(wotreplay_packet_reader_h) */
