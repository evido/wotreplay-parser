#include "fstream_ioctx.h"
#include <fstream>

using namespace wotreplay;

void putC(gdIOCtx *ctx, int a) { ((OfstreamIOCtx *)ctx)->of.put((char)a); }

int putBuf(gdIOCtx *ctx, const void *buf, int len) {
    ((OfstreamIOCtx *)ctx)->of.write((const char *)buf, len);
    return len;
}

OfstreamIOCtx::OfstreamIOCtx(std::ofstream &of) : ctx({.putC = putC, .putBuf = putBuf}), of(of) {}
