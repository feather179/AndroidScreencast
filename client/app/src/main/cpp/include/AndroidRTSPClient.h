
#ifndef SCREENCASTCLIENT_ANDROIDRTSPCLIENT_H
#define SCREENCASTCLIENT_ANDROIDRTSPCLIENT_H

#include "liveMedia.hh"

#include <condition_variable>
#include <mutex>

struct StreamClientState {
    MediaSubsessionIterator *iter = nullptr;
    MediaSession *session = nullptr;
    MediaSubsession *subSession = nullptr;
    MediaSubsession *videoSubSession = nullptr;
    MediaSubsession *audioSubSession = nullptr;
    TaskToken streamTimerTask = nullptr;
    double duration = 0.0;

    StreamClientState();

    ~StreamClientState();
};

class AndroidRTSPClient : public RTSPClient {
public:
    static AndroidRTSPClient *createNew(UsageEnvironment &env,
                                        char const *rtspURL,
                                        bool useTcp = false,
                                        int verbosityLevel = 0,
                                        char const *applicationName = nullptr,
                                        portNumBits tunnelOverHTTPPortNum = 0);

    void notifyRtspStartDone();

    void notifyRtspStartFailed();

    bool waitRtspStart(int milliseconds);

protected:
    AndroidRTSPClient(UsageEnvironment &env,
                      char const *rtspURL,
                      bool useTcp,
                      int verbosityLevel,
                      char const *applicationName,
                      portNumBits tunnelOverHTTPPortNum);

    virtual ~AndroidRTSPClient();

public:
    StreamClientState scs;
    bool mUseTcp;

private:
    enum RtspClientState {
        RTSP_CLIENT_STATE_INIT = 0,
        RTSP_CLIENT_STATE_START_FAILED,
        RTSP_CLIENT_STATE_START_DONE,
    };

    RtspClientState mState = RTSP_CLIENT_STATE_INIT;
    std::mutex mStateMutex;
    std::condition_variable mStateCv;

};


#endif //SCREENCASTCLIENT_ANDROIDRTSPCLIENT_H
