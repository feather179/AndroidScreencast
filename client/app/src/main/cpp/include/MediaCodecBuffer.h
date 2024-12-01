
#ifndef SCREENCASTCLIENT_MEDIACODECBUFFER_H
#define SCREENCASTCLIENT_MEDIACODECBUFFER_H

#include <vector>

struct MediaCodecBuffer {
    std::vector<uint8_t> buffer;
    uint64_t timestampUs;
};

#endif //SCREENCASTCLIENT_MEDIACODECBUFFER_H
