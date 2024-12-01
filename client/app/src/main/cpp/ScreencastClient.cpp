#include <jni.h>


#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "AndroidRTSPClient.h"
#include "AndroidMediaSink.h"

#include "FinalAction.h"

#include <string>
#include <sstream>
#include <ostream>

#include <android/log.h>

#define LOG_TAG "ScreencastClient-JNI"


struct fields_t {
    jmethodID lockAndGetContextID;
    jmethodID setAndUnlockContextID;
    jfieldID bufferID;
    jfieldID sizeID;
    jfieldID timestampUsID;
};

static fields_t gFields;

void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode, char *resultString);

void continueAfterSETUP(RTSPClient *rtspClient, int resultCode, char *resultString);

void continueAfterPLAY(RTSPClient *rtspClient, int resultCode, char *resultString);


// Other event handler functions:
// called when a stream's subsession (e.g., audio or video substream) ends
void subsessionAfterPlaying(void *clientData);

// called when a RTCP "BYE" is received for a subsession
void subsessionByeHandler(void *clientData, char const *reason);

// called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")
void streamTimerHandler(void *clientData);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient *rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(void *data);

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
std::ostream &operator<<(std::ostream &ss, const RTSPClient &rtspClient) {
    return ss << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
std::ostream &operator<<(std::ostream &ss, const MediaSubsession &subsession) {
    return ss << subsession.mediumName() << "/" << subsession.codecName();
}


void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode, char *resultString) {
    std::stringstream ss;

    do {
        UsageEnvironment &env = rtspClient->envir(); // alias
        StreamClientState &scs = ((AndroidRTSPClient *) rtspClient)->scs; // alias

        if (resultCode != 0) {
            ss.str("");
            ss << *rtspClient << "Failed to get a SDP description: " << resultString;
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", ss.str().c_str());
            delete[] resultString;
            break;
        }

        char *const sdpDescription = resultString;
        ss.str("");
        ss << *rtspClient << "Got a SDP description:\n" << sdpDescription;
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", ss.str().c_str());

        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(env, sdpDescription);
        delete[] sdpDescription; // because we don't need it anymore
        if (scs.session == nullptr) {
            ss.str("");
            ss << *rtspClient
               << "Failed to create a MediaSession object from the SDP description: "
               << env.getResultMsg();
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", ss.str().c_str());
            break;
        } else if (!scs.session->hasSubsessions()) {
            ss.str("");
            ss << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", ss.str().c_str());
            break;
        }

        // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
        // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
        // (Each 'subsession' will have its own data source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession(rtspClient);
        return;
    } while (false);

    // An unrecoverable error occurred with this stream.
//    shutdownStream(rtspClient);
    ((AndroidRTSPClient *) rtspClient)->notifyRtspStartFailed();
}

void setupNextSubsession(RTSPClient *rtspClient) {
    UsageEnvironment &env = rtspClient->envir(); // alias
    StreamClientState &scs = ((AndroidRTSPClient *) rtspClient)->scs; // alias
    std::stringstream ss;

    scs.subSession = scs.iter->next();
    if (scs.subSession != nullptr) {
        if (!scs.subSession->initiate()) {
            ss.str("");
            ss << *rtspClient << "Failed to initiate the \"" << *scs.subSession
               << "\" subsession: " << env.getResultMsg();
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", ss.str().c_str());
            setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
        } else {
            ss.str("");
            ss << *rtspClient << "Initiated the \"" << *scs.subSession << "\" subsession (";
            if (scs.subSession->rtcpIsMuxed()) {
                ss << "client port " << scs.subSession->clientPortNum() << ")";
            } else {
                ss << "client ports " << scs.subSession->clientPortNum() << "-"
                   << scs.subSession->clientPortNum() + 1 << ")";
            }
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", ss.str().c_str());

            // Continue setting up this subsession, by sending a RTSP "SETUP" command:
            rtspClient->sendSetupCommand(*scs.subSession,
                                         continueAfterSETUP,
                                         False,
                                         ((AndroidRTSPClient *) rtspClient)->mUseTcp);
        }
        return;
    }

    // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
    if (scs.session->absStartTime() != nullptr) {
        // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
        rtspClient->sendPlayCommand(*scs.session,
                                    continueAfterPLAY,
                                    scs.session->absStartTime(),
                                    scs.session->absEndTime());
    } else {
        scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
    }
}

void continueAfterSETUP(RTSPClient *rtspClient, int resultCode, char *resultString) {
    std::stringstream ss;

    do {
        UsageEnvironment &env = rtspClient->envir(); // alias
        StreamClientState &scs = ((AndroidRTSPClient *) rtspClient)->scs; // alias

        if (resultCode != 0) {
            ss.str("");
            ss << *rtspClient << "Failed to set up the \"" << *scs.subSession << "\" subsession: "
               << resultString;
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", ss.str().c_str());
            break;
        }

        ss.str("");
        ss << *rtspClient << "Set up the \"" << *scs.subSession << "\" subsession (";
        if (scs.subSession->rtcpIsMuxed()) {
            ss << "client port " << scs.subSession->clientPortNum() << ")";
        } else {
            ss << "client ports " << scs.subSession->clientPortNum() << "-"
               << scs.subSession->clientPortNum() + 1 << ")";
        }
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", ss.str().c_str());

        // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
        // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
        // after we've sent a RTSP "PLAY" command.)

        scs.subSession->sink = AndroidMediaSink::createNew(env, *scs.subSession, rtspClient->url());
        // perhaps use your own custom "MediaSink" subclass instead
        if (scs.subSession->sink == nullptr) {
            ss.str("");
            ss << *rtspClient << "Failed to create a data sink for the \"" << *scs.subSession
               << "\" subsession: " << env.getResultMsg();
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", ss.str().c_str());
            break;
        }

        if (std::string(scs.subSession->mediumName()) == "video") {
            scs.videoSubSession = scs.subSession;
        } else if (std::string(scs.subSession->mediumName()) == "audio") {
            scs.audioSubSession = scs.subSession;
        }

        ss.str("");
        ss << *rtspClient << "Created a data sink for the \"" << *scs.subSession << "\" subsession";
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", ss.str().c_str());

        scs.subSession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession
        scs.subSession->sink->startPlaying(*(scs.subSession->readSource()),
                                           subsessionAfterPlaying,
                                           scs.subSession);
        // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
        if (scs.subSession->rtcpInstance() != nullptr) {
            scs.subSession->rtcpInstance()->setByeWithReasonHandler(subsessionByeHandler,
                                                                    scs.subSession);
        }
    } while (false);
    delete[] resultString;

    // Set up the next subsession, if any:
    setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient *rtspClient, int resultCode, char *resultString) {
    Boolean success = False;
    std::stringstream ss;

    do {
        UsageEnvironment &env = rtspClient->envir(); // alias
        StreamClientState &scs = ((AndroidRTSPClient *) rtspClient)->scs; // alias

        if (resultCode != 0) {
            ss.str("");
            ss << *rtspClient << "Failed to start playing session: " << resultString;
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", ss.str().c_str());
            break;
        }

        // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
        // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
        // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
        // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
        if (scs.duration > 0) {
            unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
            scs.duration += delaySlop;
            unsigned uSecsToDelay = (unsigned) (scs.duration * 1000000);
            scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay,
                                                                          (TaskFunc *) streamTimerHandler,
                                                                          rtspClient);
        }

        ss.str("");
        ss << *rtspClient << "Started playing session";
        if (scs.duration > 0) {
            ss << " (for up to " << scs.duration << " seconds)";
        }
        ss << "...";
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", ss.str().c_str());

        success = True;

        ((AndroidRTSPClient *) rtspClient)->notifyRtspStartDone();
    } while (false);
    delete[] resultString;

    if (!success) {
        // An unrecoverable error occurred with this stream.
        shutdownStream(rtspClient);
    }
}

void subsessionAfterPlaying(void *clientData) {
    auto subSession = (MediaSubsession *) clientData;
    auto rtspClient = (RTSPClient *) (subSession->miscPtr);

    // Begin by closing this subSession's stream:
    Medium::close(subSession->sink);
    subSession->sink = nullptr;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession &session = subSession->parentSession();
    MediaSubsessionIterator iter(session);
    while ((subSession = iter.next()) != nullptr) {
        if (subSession->sink != nullptr) return; // this subSession is still active
    }

    // All subSessions' streams have now been closed, so shutdown the client:
    shutdownStream(rtspClient);
}

void subsessionByeHandler(void *clientData, char const *reason) {
    auto subSession = (MediaSubsession *) clientData;
    auto rtspClient = (RTSPClient *) subSession->miscPtr;
    UsageEnvironment &env = rtspClient->envir(); // alias
    std::stringstream ss;

    ss.str("");
    ss << *rtspClient << "Received RTCP \"BYE\"";
    if (reason != NULL) {
        ss << " (reason:\"" << reason << "\")";
        delete[] (char *) reason;
    }
    ss << " on \"" << *subSession << "\" subSession";
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", ss.str().c_str());

    // Now act as if the subSession had closed:
    subsessionAfterPlaying(subSession);
}

void streamTimerHandler(void *clientData) {
    auto rtspClient = reinterpret_cast<AndroidRTSPClient *>(clientData);
    StreamClientState &scs = rtspClient->scs; // alias

    scs.streamTimerTask = nullptr;

    // Shut down the stream:
    shutdownStream(rtspClient);
}

void shutdownStream(void *data) {
    auto rtspClient = reinterpret_cast<RTSPClient *>(data);
    UsageEnvironment &env = rtspClient->envir(); // alias
    StreamClientState &scs = ((AndroidRTSPClient *) rtspClient)->scs; // alias
    std::stringstream ss;

    // First, check whether any subsessions have still to be closed:
    if (scs.session != nullptr) {
        Boolean someSubsessionsWereActive = False;
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession *subsession;

        while ((subsession = iter.next()) != nullptr) {
            if (subsession->sink != nullptr) {
                Medium::close(subsession->sink);
                subsession->sink = nullptr;

                if (subsession->rtcpInstance() != nullptr) {
                    // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
                    subsession->rtcpInstance()->setByeHandler(nullptr, nullptr);
                }

                someSubsessionsWereActive = True;
            }
        }

        if (someSubsessionsWereActive) {
            // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
            // Don't bother handling the response to the "TEARDOWN".
            rtspClient->sendTeardownCommand(*scs.session, nullptr);
        }
    }

    ss.str("");
    ss << *rtspClient << "Closing the stream.";
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", ss.str().c_str());
    Medium::close(rtspClient);
    // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.

//    if (--rtspClientCount == 0) {
//        // The final stream has ended, so exit the application now.
//        // (Of course, if you're embedding this code into your own application, you might want to comment this out,
//        // and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
//        exit(exitCode);
//    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_xingyu_screencastclient_MainActivity_nativeInit(JNIEnv *env, jobject thiz) {
    jclass clazz = env->FindClass("com/xingyu/screencastclient/VideoPlayActivity");

    gFields.lockAndGetContextID = env->GetMethodID(clazz, "lockAndGetContext", "()J");
    gFields.setAndUnlockContextID = env->GetMethodID(clazz, "setAndUnlockContext", "(J)V");
    env->DeleteLocalRef(clazz);

    clazz = env->FindClass("com/xingyu/screencastclient/MediaCodecBuffer");
    gFields.bufferID = env->GetFieldID(clazz, "buffer", "Ljava/nio/ByteBuffer;");
    gFields.sizeID = env->GetFieldID(clazz, "size", "I");
    gFields.timestampUsID = env->GetFieldID(clazz, "timestampUs", "J");
    env->DeleteLocalRef(clazz);
}


#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"
char eventLoopWatchVariable = 0;
extern "C"
JNIEXPORT void JNICALL
Java_com_xingyu_screencastclient_VideoPlayActivity_nativeOpenRtsp(JNIEnv *jniEnv, jobject thiz,
                                                                  jstring rtsp_addr,
                                                                  jboolean is_tcp) {
    // Begin by setting up our usage environment:
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *env = BasicUsageEnvironment::createNew(*scheduler);

    const char *str = jniEnv->GetStringUTFChars(rtsp_addr, nullptr);
    std::string url = str;
    jniEnv->ReleaseStringUTFChars(rtsp_addr, str);

    auto rtspClient = AndroidRTSPClient::createNew(*env,
                                                   url.c_str(),
                                                   is_tcp,
                                                   RTSP_CLIENT_VERBOSITY_LEVEL,
                                                   "ScreencastClient");
    rtspClient->sendDescribeCommand(continueAfterDESCRIBE);

    jniEnv->CallLongMethod(thiz, gFields.lockAndGetContextID);
    jniEnv->CallVoidMethod(thiz, gFields.setAndUnlockContextID,
                           reinterpret_cast<uintptr_t>(rtspClient));

    // All subsequent activity takes place within the event loop:
    env->taskScheduler().doEventLoop(&eventLoopWatchVariable);
    // This function call does not return, unless, at some point in time, "eventLoopWatchVariable" gets set to something non-zero.

    // If you choose to continue the application past this point (i.e., if you comment out the "return 0;" statement above),
    // and if you don't intend to do anything more with the "TaskScheduler" and "UsageEnvironment" objects,
    // then you can also reclaim the (small) memory used by these objects by uncommenting the following code:
    /*
      env->reclaim(); env = NULL;
      delete scheduler; scheduler = NULL;
    */
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_xingyu_screencastclient_VideoPlayActivity_nativeWaitRtspStart(JNIEnv *env, jobject thiz,
                                                                       jint milliseconds) {
    jlong pointerValue = env->CallLongMethod(thiz, gFields.lockAndGetContextID);
    auto action = final_action([=]() {
        env->CallVoidMethod(thiz, gFields.setAndUnlockContextID, pointerValue);
    });

    auto rtspClient = reinterpret_cast<AndroidRTSPClient *>(pointerValue);
    return rtspClient->waitRtspStart(milliseconds);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_xingyu_screencastclient_VideoPlayActivity_nativeGetMimeType(JNIEnv *env, jobject thiz) {
    jlong pointerValue = env->CallLongMethod(thiz, gFields.lockAndGetContextID);
    auto action = final_action([=]() {
        env->CallVoidMethod(thiz, gFields.setAndUnlockContextID, pointerValue);
    });

    auto rtspClient = reinterpret_cast<AndroidRTSPClient *>(pointerValue);
    auto subSession = rtspClient->scs.videoSubSession;
    char mimetype[50];
    snprintf(mimetype, sizeof(mimetype), "%s/%s",
             subSession->mediumName(), subSession->codecName());

    return env->NewStringUTF(mimetype);
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_com_xingyu_screencastclient_VideoPlayActivity_nativeGetInputBuffer(JNIEnv *env, jobject thiz,
                                                                        jobject buffer) {
    jlong pointerValue = env->CallLongMethod(thiz, gFields.lockAndGetContextID);
    auto action = final_action([=]() {
        env->CallVoidMethod(thiz, gFields.setAndUnlockContextID, pointerValue);
    });

    auto rtspClient = reinterpret_cast<AndroidRTSPClient *>(pointerValue);
    auto mediaSink = reinterpret_cast<AndroidMediaSink *>(rtspClient->scs.videoSubSession->sink);
    auto mediaCodecBuffer = mediaSink->getNextFrame();

    if (mediaCodecBuffer == nullptr)
        return false;

    jobject nativeBuffer = env->GetObjectField(buffer, gFields.bufferID);
    auto bufferPtr = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(nativeBuffer));
    memcpy(bufferPtr, mediaCodecBuffer->buffer.data(), mediaCodecBuffer->buffer.size());
    env->SetIntField(buffer, gFields.sizeID, (jint) mediaCodecBuffer->buffer.size());
    env->SetLongField(buffer, gFields.timestampUsID, (jlong) mediaCodecBuffer->timestampUs);

    return true;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_xingyu_screencastclient_VideoPlayActivity_nativeStopGetInputBuffer(JNIEnv *env,
                                                                            jobject thiz) {
    jlong pointerValue = env->CallLongMethod(thiz, gFields.lockAndGetContextID);
    auto action = final_action([=]() {
        env->CallVoidMethod(thiz, gFields.setAndUnlockContextID, pointerValue);
    });

    auto rtspClient = reinterpret_cast<AndroidRTSPClient *>(pointerValue);
    auto mediaSink = reinterpret_cast<AndroidMediaSink *>(rtspClient->scs.videoSubSession->sink);
    mediaSink->stopGetFrame();
}
extern "C"
JNIEXPORT void JNICALL
Java_com_xingyu_screencastclient_VideoPlayActivity_nativeCloseRtsp(JNIEnv *env, jobject thiz) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "nativeCloseRtsp");

    jlong pointerValue = env->CallLongMethod(thiz, gFields.lockAndGetContextID);
    auto action = final_action([=]() {
        env->CallVoidMethod(thiz, gFields.setAndUnlockContextID, 0);
    });

    if (pointerValue == 0) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "RTSPClient = null, skip shutdown stream");
        return;
    }

    auto rtspClient = reinterpret_cast<AndroidRTSPClient *>(pointerValue);
    rtspClient->envir().taskScheduler().scheduleDelayedTask(0, shutdownStream, rtspClient);
}