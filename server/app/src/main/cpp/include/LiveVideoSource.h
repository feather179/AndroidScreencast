
#ifndef SCREENCAST_SERVER_LIVEVIDEOSOURCE_H
#define SCREENCAST_SERVER_LIVEVIDEOSOURCE_H

#include <FramedSource.hh>
#include "Encoder.h"

#include <vector>
#include <queue>
#include <mutex>

class LiveVideoSource : public FramedSource {
public:
    static LiveVideoSource *createNew(UsageEnvironment &env, std::shared_ptr<Encoder> encoder);

    void queueBuffer(std::shared_ptr<std::vector<uint8_t>> buffer, bool isCsd, bool isKeyFrame);

    unsigned maxFrameSize() const override;

protected:
    LiveVideoSource(UsageEnvironment &env, std::shared_ptr<Encoder> encoder);

    virtual ~LiveVideoSource();

private:
    void doGetNextFrame() override;

    static void deliverFrame(void *clientData);

    void deliverFrameInternal();

private:
    EventTriggerId mEventTriggerId;
    std::queue<std::shared_ptr<std::vector<uint8_t>>> mBufferQueue;
    std::mutex mBufferQueueMutex;

    std::shared_ptr<Encoder> mEncoder = nullptr;
    bool mReceivedKeyFrame = false;

};

#endif //SCREENCAST_SERVER_LIVEVIDEOSOURCE_H
