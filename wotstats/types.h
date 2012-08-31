//
//  types.h
//  wotstats
//
//  Created by Jan Temmerman on 31/08/12.
//
//

#ifndef wotstats_types_h
#define wotstats_types_h

#include <vector>
#include <boost/range/iterator_range.hpp>

namespace wotstats {
    typedef std::vector<uint8_t> buffer_t;
    typedef boost::iterator_range<buffer_t::iterator> slice_t;
}

#endif
