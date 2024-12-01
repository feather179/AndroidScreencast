
#ifndef SCREENCAST_SERVER_ENCODER_H
#define SCREENCAST_SERVER_ENCODER_H

#include <functional>
#include <memory>
#include <vector>

class Encoder {
public:
    enum EncoderState {
        ENCODER_STATE_STARTED,
        ENCODER_STATE_STOPPED,
    };

    Encoder() = default;

    virtual ~Encoder() = default;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual EncoderState getState();

    virtual void
    setOnBufferAvailCB(std::function<void(std::shared_ptr<std::vector<uint8_t>>, bool, bool)> cb);

    virtual void
    queueBuffer(std::shared_ptr<std::vector<uint8_t>> buffer, bool isCsd, bool isKeyFrame);

    virtual void getCsd(std::vector<uint8_t> &csd);

protected:
    EncoderState mEncoderState = ENCODER_STATE_STOPPED;
    std::function<void(std::shared_ptr<std::vector<uint8_t>>, bool, bool)> mBufferAvailCB = nullptr;
    std::vector<uint8_t> mCsd;
};


#endif //SCREENCAST_SERVER_ENCODER_H
