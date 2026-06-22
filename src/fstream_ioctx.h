#ifndef fstream_ioctx_h
#define fstream_ioctx_h

#include "gd_io.h"
#include <iosfwd>

namespace wotreplay {

struct OfstreamIOCtx {
    OfstreamIOCtx(std::ofstream &of);
    gdIOCtx ctx;
    std::ofstream &of;
};

} // namespace wotreplay

#endif
