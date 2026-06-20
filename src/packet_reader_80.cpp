#include "packet_reader_80.h"
#include "logger.h"
#include "packet.h"
#include "types.h"

#include <boost/format.hpp>
#include <cstdint>
#include <stdexcept>

using namespace wotreplay;

void packet_reader_80_t::init(const version_t &version, buffer_t *buffer, game_title_t title) {
    this->buffer = buffer;
    this->version = version;
    if (init_pos > 0) {
        // 38 = 8 (header) + 4 (size) + (size) + 22 (unknown)
        this->pos = ((int)(*buffer)[8]) + 38;
    }
    this->title = title;
}

packet_t packet_reader_80_t::next() {
    const int base_packet_size = 12;
    int payload_size = *reinterpret_cast<int *>(&((*buffer)[pos]));
    int packet_size = payload_size + base_packet_size;

    if ((pos + packet_size) > buffer->size()) {
        throw std::runtime_error("packet outside of bounds");
    }

    auto packet_begin = buffer->begin() + pos;
    auto packet_end = packet_begin + packet_size;

    packet_t packet(pos, boost::make_iterator_range(packet_begin, packet_end), this->init_pos != 0);

    logger.writef(log_level_t::debug, "[%1%] type=0x%2$02X ", pos, packet.type());

    logger.writef(log_level_t::debug, "size=%1% data=%2% ", packet_size, packet);

    if (packet.has_property(property_t::player_id)) {
        logger.writef(log_level_t::debug, "player_id=%1% ", packet.player_id());
    }

    if (packet.has_property(property_t::sub_type)) {
        logger.writef(log_level_t::debug, "sub_type=%1% ", (int)packet.sub_type());
    }

    if (packet.has_property(property_t::player_name)) {
        logger.writef(log_level_t::debug, "player_name=%1% team_id=%2$02X ", packet.player_name(), (int)packet.team_id());
    }

    if (packet.has_property(property_t::health)) {
        logger.writef(log_level_t::debug, "health=%1% ", packet.health());
    }

    if (packet.has_property(property_t::source)) {
        logger.writef(log_level_t::debug, "source=%1% ", (int)packet.source());
    }

    if (packet.has_property(property_t::clock)) {
        logger.writef(log_level_t::debug, "clock=%1% ", packet.clock());
    }

    if (packet.has_property(property_t::target)) {
        logger.writef(log_level_t::debug, "target=%1% ", (int)packet.target());
    }

    if (packet.has_property(property_t::max_health)) {
        logger.writef(log_level_t::debug, "max_health=%1% ", (int)packet.max_health());
    }

    if (packet.type() == 0x08 && packet.sub_type() == 20 && packet.blitz_packet) {
        logger.writef(log_level_t::debug, "hit_position=[ %1%, %2%, %3% ] ",
                      packet.get_data_field<float>(28),
                      packet.get_data_field<float>(32),
                      packet.get_data_field<float>(36)
                      );
    }

    if (packet.type() == 0x05 && packet.sub_type() == 0x02) {
        const uint32_t field_base = 55;

        const int32_t field_sizes[] = {
            2, 2, 3, 3, 3, -5, -6, 2, 2, 2, -10, 5, 2, 3, 3, 9, 2, 2,
        };

        uint32_t field_index = 0;
        uint32_t field_offset = field_base;
        for (int i = 0; i < sizeof(field_sizes) / sizeof(field_sizes[0]); i += 1) {
            if (packet.get_data_field<int8_t>(field_offset) != i) {
                logger.writef(log_level_t::debug, "f%1%=error ", field_index);
                break;
            }

            int field_size = field_sizes[i];

            switch (i) {
            case 0x05:
                field_size = 23 + packet.get_data_field<int8_t>(field_offset + 1);
                break;
            case 0x06:
                field_size = 2 + packet.get_data_field<int8_t>(field_offset + 1) * 16;
                break;
            case 0x0A:
                field_size = 2 + packet.get_data_field<int8_t>(field_offset + 1) * 14;
                break;
            case 0x07:
            case 0x08:
            case 0x09:
            case 0x0B:
            case 0x0C:
            case 0x10:
            case 0x11:
                field_size = 2 + packet.get_data_field<int8_t>(field_offset + 1);
                break;
            default:
                field_size = field_sizes[i];
                break;
            };

            const slice_t field_data = {packet.get_data().begin() + field_offset, packet.get_data().begin() + field_offset + field_size};

            logger.writef(log_level_t::debug, "f%1%=%2% ", field_index, to_string(field_data));

            field_offset += field_size;
            field_index += 1;
        }
    }

    logger.writef(log_level_t::debug, "\n");

    prev = pos;
    pos += packet_size;

    return packet;
}

bool packet_reader_80_t::has_next() {
    bool has_next = pos < buffer->size();
    if (!has_next) {
        // check if we ended with an end marker type block
        const int end_marker = 0xFFFFFFFF;
        int type = *(reinterpret_cast<int *>(&((*buffer)[prev])) + 1);
        if (type != end_marker) {
            logger.write(log_level_t::warning, "packet stream did not end with end marker type block\n");
        }
    }
    return has_next;
}

bool packet_reader_80_t::is_compatible(const version_t &version) { return version.major >= 8; }
