#ifndef wotreplay_types_h
#define wotreplay_types_h

#include <vector>
#include <boost/range/iterator_range.hpp>

/** @file types.h */

namespace wotreplay {
    /**
     * @typedef typedef std::vector<uint8_t> wot::buffer_t
     * @brief Definition of buffer_t.
     */
    typedef std::vector<uint8_t> buffer_t;
    /**
     * @typedef typedef boost::iterator_range<buffer_t::iterator> wot::slice_t
     * @brief Definition of slice_t.
     */
    typedef boost::iterator_range<buffer_t::iterator> slice_t;
}

#endif /* defined(wotreplay_types_h) */
