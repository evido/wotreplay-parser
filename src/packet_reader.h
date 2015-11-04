#ifndef wotreplay_packet_reader_h
#define wotreplay_packet_reader_h

#include "game.h"
#include "types.h"

namespace wotreplay {
	class packet_t;

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
        /**
         * Determines if the reader is compatible with the given version
         * @param version the game version of the replay buffer
         * @return \c true if the reader is compatible with version \c false if not
         */
        virtual bool is_compatible(const version_t &version) = 0;
        /**
         * Initializes the packet reader
         * @param version the game version of the replay buffer
         * @param buffer the replay buffer
         */
        virtual void init(const version_t &version, buffer_t *buffer, game_title_t title) = 0;
        virtual ~packet_reader_t() {}
    };
}

#endif /* defined(wotreplay_packet_reader_h) */
