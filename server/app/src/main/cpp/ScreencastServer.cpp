#include <jni.h>

#include "LiveVideoSource.h"
#include "H265LiveVideoServerMediaSubsession.h"
#include "AndroidUsageEnvironment.h"
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "Encoder.h"

#include <sstream>
#include <iomanip>

#include <android/log.h>

#define LOG_TAG "ScreencastServer-JNI"


struct fields_t {
    jmethodID lockAndGetContextID;
    jmethodID setAndUnlockContextID;
    jmethodID startID;
    jmethodID stopID;
    jmethodID joinID;
    jmethodID requestSyncFrameID;
};

static fields_t gFields;

static JNIEnv *getJNIEnv(JavaVM *jvm) {
    JNIEnv *jniEnv = nullptr;

    if (int status = jvm->GetEnv((void **) &jniEnv, JNI_VERSION_1_6) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to get jniEnv, status:%d", status);
        return nullptr;
    }
    return jniEnv;
}

class VideoEncoder : public Encoder {
public:
    VideoEncoder(JNIEnv *env, jobject thiz, void *rtspServer) : mRtspServer(rtspServer) {
        env->GetJavaVM(&mJvm);
        jclass clazz = env->GetObjectClass(thiz);
        mClass = (jclass) env->NewGlobalRef(clazz);
        mObject = env->NewWeakGlobalRef(thiz);
    }

    virtual ~VideoEncoder() {
        JNIEnv *jniEnv = getJNIEnv(mJvm);
        if (jniEnv) {
            jniEnv->DeleteWeakGlobalRef(mObject);
            mObject = nullptr;
            jniEnv->DeleteGlobalRef(mClass);
            mClass = nullptr;
        }
    }

    void start() override {
        JNIEnv *jniEnv = getJNIEnv(mJvm);
//        jniEnv->CallVoidMethod(mObject, gFields.startID);

        jniEnv->CallVoidMethod(mObject, gFields.requestSyncFrameID);

        mEncoderState = ENCODER_STATE_STARTED;
    }

    void stop() override {
//        JNIEnv *jniEnv = getJNIEnv(mJvm);
//        jniEnv->CallVoidMethod(mObject, gFields.stopID);
//        jniEnv->CallVoidMethod(mObject, gFields.joinID);

        mEncoderState = ENCODER_STATE_STOPPED;
    }

    void *getRtspServer() {
        return mRtspServer;
    }

    char mEncoderStopFlag = 0;

private:
    JavaVM *mJvm = nullptr;
    jclass mClass = nullptr;
    jweak mObject = nullptr;
    void *mRtspServer = nullptr;
};

static void
setNativeVideoEncoder(JNIEnv *env, jobject thiz, std::shared_ptr<VideoEncoder> encoder) {
    env->CallLongMethod(thiz, gFields.lockAndGetContextID);
    auto encoderPointer = new std::shared_ptr<VideoEncoder>(encoder);
    env->CallVoidMethod(thiz, gFields.setAndUnlockContextID, encoderPointer);
}

static std::shared_ptr<VideoEncoder> getNativeVideoEncoder(JNIEnv *env, jobject thiz) {
    jlong pointerValue = env->CallLongMethod(thiz, gFields.lockAndGetContextID);
    if (pointerValue == 0) {
        env->CallVoidMethod(thiz, gFields.setAndUnlockContextID, 0);
        return nullptr;
    } else {
        auto encoder = *(std::shared_ptr<VideoEncoder> *) pointerValue;
        env->CallVoidMethod(thiz, gFields.setAndUnlockContextID, pointerValue);
        return encoder;
    }
}

static void releaseNativeEncoder(JNIEnv *env, jobject thiz) {
    jlong pointerValue = env->CallLongMethod(thiz, gFields.lockAndGetContextID);
    if (pointerValue != 0) {
        delete ((std::shared_ptr<VideoEncoder> *) pointerValue);
    }
    env->CallVoidMethod(thiz, gFields.setAndUnlockContextID, 0);
}

static void
queueBufferToNativeEncoder(JNIEnv *env, jobject thiz, jobject buffer, int size, jboolean isCsd,
                           jboolean isKeyFrame) {
    auto encoder = getNativeVideoEncoder(env, thiz);

    if (encoder == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "queueBufferToNativeEncoder: Can not get native video encoder");
        return;
    }

    if (encoder->getState() != Encoder::ENCODER_STATE_STARTED && isCsd == false)
        return;

    auto bufferPtr = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(buffer));
    auto v = std::make_shared<std::vector<uint8_t>>();
    v->insert(v->end(), bufferPtr, bufferPtr + size);

    encoder->queueBuffer(v, isCsd, isKeyFrame);
}

static void announceStream(RTSPServer *rtspServer, ServerMediaSession *sms,
                           char const *streamName, char const *inputFileName) {
    UsageEnvironment &env = rtspServer->envir();
    std::stringstream ss;

    ss << "\"" << streamName << "\" stream, from the file \"" << inputFileName << "\"";
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", ss.str().c_str());

    ss.str("");
    ss << "Play this stream using the URL ";

    if (weHaveAnIPv4Address(env)) {
        char *url = rtspServer->ipv4rtspURL(sms);
        ss << "\"" << url << "\"";
        delete[] url;
    }
    if (weHaveAnIPv6Address(env)) {
        char *url = rtspServer->ipv6rtspURL(sms);
        ss << " or " << "\"" << url << "\"";
        delete[] url;
    }
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", ss.str().c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_xingyu_screencastserver_MainActivity_nativeInit(JNIEnv *env, jobject thiz) {
    jclass clazz = env->FindClass("com/xingyu/screencastserver/video/VideoEncoder");

    gFields.lockAndGetContextID = env->GetMethodID(clazz, "lockAndGetContext", "()J");
    gFields.setAndUnlockContextID = env->GetMethodID(clazz, "setAndUnlockContext", "(J)V");
    gFields.startID = env->GetMethodID(clazz, "start", "()V");
    gFields.stopID = env->GetMethodID(clazz, "stop", "()V");
    gFields.joinID = env->GetMethodID(clazz, "join", "()V");
    gFields.requestSyncFrameID = env->GetMethodID(clazz, "requestSyncFrame", "()V");

    env->DeleteLocalRef(clazz);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_xingyu_screencastserver_RtspService_nativeStartServer(JNIEnv *jniEnv, jobject thiz,
                                                               jobject encoder) {
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *env = AndroidUsageEnvironment::createNew(*scheduler);
    RTSPServer *rtspServer = RTSPServer::createNew(*env, 8554, nullptr);
    auto videoEncoder = std::make_shared<VideoEncoder>(jniEnv, encoder, rtspServer);
    setNativeVideoEncoder(jniEnv, encoder, videoEncoder);

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "errno:%s", strerror(errno));

    OutPacketBuffer::maxSize = 2000000;

    char const *descriptionString = "Session streamed by \"ScreencastServer\"";
    // A H.265 video elementary stream:
    {
        char const *streamName = "h265";
        char const *inputFileName = "test.265";
        ServerMediaSession *sms
                = ServerMediaSession::createNew(*env, streamName, streamName, descriptionString);
        sms->addSubsession(H265LiveVideoServerMediaSubsession::createNew(*env, videoEncoder));
        rtspServer->addServerMediaSession(sms);

        announceStream(rtspServer, sms, streamName, inputFileName);

        {
            jclass clazz = jniEnv->GetObjectClass(thiz);
            jmethodID methodId = jniEnv->GetMethodID(clazz, "announceStream",
                                                     "(Ljava/lang/String;)V");
            char *url = rtspServer->ipv4rtspURL(sms);
            jstring jUrl = jniEnv->NewStringUTF(url);
            jniEnv->CallVoidMethod(thiz, methodId, jUrl);
            jniEnv->DeleteLocalRef(jUrl);
            delete[] url;
        }
    }

    env->taskScheduler().doEventLoop(&(videoEncoder->mEncoderStopFlag));
}
extern "C"
JNIEXPORT void JNICALL
Java_com_xingyu_screencastserver_RtspService_nativeStopServer(JNIEnv *env, jobject thiz,
                                                              jobject encoder) {
    auto nativeEncoder = getNativeVideoEncoder(env, encoder);
    if (nativeEncoder != nullptr) {
        nativeEncoder->mEncoderStopFlag = ~0;
        Medium::close(reinterpret_cast<RTSPServer *>(nativeEncoder->getRtspServer()));
    }

    releaseNativeEncoder(env, encoder);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_xingyu_screencastserver_RtspService_nativeQueueBuffer(JNIEnv *env, jobject thiz,
                                                               jobject encoder, jobject buffer,
                                                               jint size, jboolean isCsd,
                                                               jboolean isKeyFrame) {
    queueBufferToNativeEncoder(env, encoder, buffer, size, isCsd, isKeyFrame);
}