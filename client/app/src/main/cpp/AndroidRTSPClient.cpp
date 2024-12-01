
#include "AndroidRTSPClient.h"

#include <android/log.h>

#define LOG_TAG "AndroidRTSPClient"

StreamClientState::StreamClientState() {

}

StreamClientState::~StreamClientState() {
    delete iter;

    if (session != nullptr) {
        // We also need to delete "session", and unschedule "streamTimerTask" (if set)
        UsageEnvironment &env = session->envir(); // alias

        env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
        Medium::close(session);
    }
}

AndroidRTSPClient *AndroidRTSPClient::createNew(UsageEnvironment &env,
                                                const char *rtspURL,
                                                bool useTcp,
                                                int verbosityLevel,
                                                const char *applicationName,
                                                portNumBits tunnelOverHTTPPortNum) {
    return new AndroidRTSPClient(env,
                                 rtspURL,
                                 useTcp,
                                 verbosityLevel,
                                 applicationName,
                                 tunnelOverHTTPPortNum);
}

AndroidRTSPClient::AndroidRTSPClient(UsageEnvironment &env,
                                     const char *rtspURL,
                                     bool useTcp,
                                     int verbosityLevel,
                                     const char *applicationName,
                                     portNumBits tunnelOverHTTPPortNum)
        : RTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1),
          mUseTcp(useTcp) {

}

AndroidRTSPClient::~AndroidRTSPClient() {

}

void AndroidRTSPClient::notifyRtspStartDone() {
    std::unique_lock<std::mutex> lk(mStateMutex);
    mState = RTSP_CLIENT_STATE_START_DONE;
    mStateCv.notify_all();
}

void AndroidRTSPClient::notifyRtspStartFailed() {
    std::unique_lock<std::mutex> lk(mStateMutex);
    mState = RTSP_CLIENT_STATE_START_FAILED;
    mStateCv.notify_all();
}

// return false if wait timeout
bool AndroidRTSPClient::waitRtspStart(int milliseconds) {
    std::unique_lock<std::mutex> lk(mStateMutex);
    auto futureTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (mState == RTSP_CLIENT_STATE_INIT) {
        if (std::cv_status::timeout == mStateCv.wait_until(lk, futureTime))
            break;
    }

    return (mState == RTSP_CLIENT_STATE_START_DONE);
}
