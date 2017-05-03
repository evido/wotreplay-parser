#include "logger.h"
#include "packet_reader_80.h"

#include <boost/format.hpp>

using namespace wotreplay;

void packet_reader_80_t::init(const version_t &version, buffer_t *buffer, game_title_t title) {
    this->buffer = buffer;
    this->version = version;
    this->pos = 0;
	this->title = title;
}


packet_t packet_reader_80_t::next() {
    const int base_packet_size = 12;
    int payload_size = *reinterpret_cast<int*>(&((*buffer)[pos]));
    int packet_size = payload_size + base_packet_size;
    
    if ((pos + packet_size) > buffer->size()) {
        throw std::runtime_error("packet outside of bounds");
    }

    auto packet_begin = buffer->begin() + pos;
    auto packet_end = packet_begin + packet_size;

    packet_t packet( boost::make_iterator_range(packet_begin, packet_end) );
    logger.writef(wotreplay::log_level_t::debug,
                    "[%2%] type=0x%1$02X size=%3%\n%4%\n",
                    packet.type(), pos, packet_size, packet);
    
    prev = pos;
    pos += packet_size;
    
    return packet;
}

bool packet_reader_80_t::has_next() {
    bool has_next = pos < buffer->size();
    if (!has_next) {
        // check if we ended with an end marker type block
        const int end_marker = 0xFFFFFFFF;
        int type = *(reinterpret_cast<int*>(&((*buffer)[prev])) + 1);
        if (type != end_marker) {
            logger.write(log_level_t::warning, "packet stream did not end with end marker type block\n");
        }
    
    }
    return has_next;
}

bool packet_reader_80_t::is_compatible(const version_t &version) {
    return version.major >= 8;
}