#include "packet_reader_80.h"

#include <boost/format.hpp>

using namespace wotreplay;

static std::map<uint8_t, packet_config_t> base_packet_configs = {
    {0x00, {22, 15, 4}},
    {0x03, {24,  0, 0}},
    {0x04, {16,  0, 0}},
    {0x05, {54, 47, 1}},
    {0x07, {24, 17, 2}},
    {0x08, {24, 17, 2}},
    {0x0A, {61,  0, 0}},
    {0x0B, {30, 23, 1}},
    {0x0E, {25, 10, 4}}, // changed {0x0E, 4}
    {0x0C, { 3,  0, 0}},
    {0x11, {12,  0, 0}},
    {0x12, {16,  0, 0}},
    {0x13, {16,  0, 0}},
    {0x14, { 4,  0, 0}},
    {0x15, {44,  0, 0}},
    {0x16, {80,  0, 0}}, // < 0.8.5 = {0x16, 52}
    {0x17, {16,  9, 1}},
    {0x18, {16,  0, 0}},
    {0x19, {16,  0, 0}},
    {0x1B, {16,  0, 0}},
    {0x1C, {20,  0, 0}},
    {0x1D, {21,  0, 0}},
    {0x1A, {16,  0, 0}},
    {0x1E, {16,  0, 0}},  // modified for 0.7.2 {0x1e, 160},
    {0x1F, {17,  9, 1}},
    {0x20, {21, 14, 1}},  // modified for 0.8.0 {0x20, 4}
    {0x31, { 4,  0, 0}},  // indication of restart of the replay, probably not a part of the replay but necessary because of wrong detection of the start of the replay
    {0x0D, {22, 15, 4}}
};

static std::array<uint8_t, 6> marker = {{0x2C, 0x01, 0x01, 0x00, 0x00, 0x00}};

void packet_reader_80_t::init(const version_t &version, buffer_t *buffer)
{
    this->packet_configs = base_packet_configs;
    this->buffer= buffer;
    this->version = version;
    this->pos = 0;

    if (!(version.major == 8 && version.minor >= 5)
            || version.major < 8) {
        packet_configs[0x16] = {52, 0, 0};
    }

    if (version.major < 8) {
        packet_configs[0x20] = { 4, 0, 0};
    }

    // if (is_legacy()) {
    //     packet_lengths[0x16] = 44;
    // }

    auto start = std::search(buffer->begin(), buffer->end(),marker.begin(), marker.end());
    if (start != buffer->end()) {
        pos = static_cast<int>(std::distance(buffer->begin(), start) + marker.size());
//        if (debug) {
//            std::cerr << "OFFSET: " << offset << "\n";
//        }
    }
}

packet_t packet_reader_80_t::next() {

    auto it = base_packet_configs.find((*buffer)[pos + 1]);
    if (it == base_packet_configs.end()) {
        throw std::runtime_error("no such packet type");
    }

    packet_config_t packet_config  = it->second;
    
    int total_packet_size = packet_config.size;

    if (packet_config.payload_length_offset > 0) {
        switch(packet_config.payload_length_type) {
        case 1:
            total_packet_size += get_field<uint8_t>(buffer->begin(), buffer->end(),
                                                    pos + packet_config.payload_length_offset);
            break;
        case 2:
            total_packet_size += get_field<uint16_t>(buffer->begin(), buffer->end(),
                                                     pos + packet_config.payload_length_offset);
            break;
        case 4:
            total_packet_size += get_field<uint32_t>(buffer->begin(), buffer->end(),
                                                     pos + packet_config.payload_length_offset);
            break;
        default:
            throw std::runtime_error("illegal payload_size_type");
        }
        
    }

    // include 25 byte ?
    if ((pos + 25 + total_packet_size) > buffer->size()) {
        throw std::runtime_error("packet outside of bounds");
    }

    //    if (debug) {
    //        std::cerr << boost::format("[%2%] type=0x%1$02X size=%3%\n") % (int) buffer[pos + 1] % pos % total_packet_size;
    //    }

    auto packet_begin = buffer->begin() + pos;
    auto packet_end = packet_begin + total_packet_size;

    pos += total_packet_size;
    
    return packet_t( boost::make_iterator_range(packet_begin, packet_end) );
}

bool packet_reader_80_t::has_next() {
    // in normal circumstances the buffer ends with 25 remaining bytes
    return (pos + 25) < buffer->size();
}

bool packet_reader_80_t::is_compatible(const version_t &version) {
    return version.major == 8 && (version.minor >= 0 && version.minor <= 5);
}