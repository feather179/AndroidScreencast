
#ifndef SCREENCASTCLIENT_ANDROIDMEDIASINK_H
#define SCREENCASTCLIENT_ANDROIDMEDIASINK_H

#include "MediaSink.hh"
#include "MediaSession.hh"
#include "MediaCodecBuffer.h"

#include <vector>
#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

class AndroidMediaSink : public MediaSink {
public:
    static AndroidMediaSink *createNew(UsageEnvironment &env,
                                       MediaSubsession &subsession, // identifies the kind of data that's being received
                                       char const *streamId = nullptr); // identifies the stream itself (optional)

    std::shared_ptr<MediaCodecBuffer> getNextFrame();

    void stopGetFrame();

private:
    AndroidMediaSink(UsageEnvironment &env, MediaSubsession &subsession, char const *streamId);

    // called only by "createNew()"
    virtual ~AndroidMediaSink();

    static void afterGettingFrame(void *clientData, unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned durationInMicroseconds);

    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                           struct timeval presentationTime, unsigned durationInMicroseconds);

private:
    // redefined virtual functions:
    Boolean continuePlaying() override;

private:
    std::vector<uint8_t> mReceiveBuffer;
    MediaSubsession &mSubsession;
    std::string mStreamId;

    std::queue<std::shared_ptr<MediaCodecBuffer>> mBufferQueue;
    std::mutex mBufferQueueMutex;
    std::condition_variable mBufferQueueCv;
    // default save frames when connection setup
    // when resume from paused state, do NOT save frames before mediacodec ready
    bool mStartGetFrame = true;
};


#endif //SCREENCASTCLIENT_ANDROIDMEDIASINK_H
