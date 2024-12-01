
#ifndef SCREENCAST_SERVER_H265LIVEVIDEOSERVERMEDIASUBSESSION_H
#define SCREENCAST_SERVER_H265LIVEVIDEOSERVERMEDIASUBSESSION_H

#include <OnDemandServerMediaSubsession.hh>
#include "Encoder.h"

class H265LiveVideoServerMediaSubsession : public OnDemandServerMediaSubsession {
public:
    static H265LiveVideoServerMediaSubsession *
    createNew(UsageEnvironment &env, std::shared_ptr<Encoder> encoder);

    void checkForAuxSDPLineInternal();

    void afterPlayingDummyInternal();

protected:
    H265LiveVideoServerMediaSubsession(UsageEnvironment &env, std::shared_ptr<Encoder> encoder);

    virtual ~H265LiveVideoServerMediaSubsession();

    FramedSource *createNewStreamSource(unsigned clientSessionId,
                                        unsigned &estBitrate) override;

    // "estBitrate" is the stream's estimated bitrate, in kbps
    RTPSink *createNewRTPSink(Groupsock *rtpGroupsock,
                              unsigned char rtpPayloadTypeIfDynamic,
                              FramedSource *inputSource) override;

//    void startStream(unsigned clientSessionId, void *streamToken,
//                     TaskFunc *rtcpRRHandler,
//                     void *rtcpRRHandlerClientData,
//                     unsigned short &rtpSeqNum,
//                     unsigned &rtpTimestamp,
//                     ServerRequestAlternativeByteHandler *serverRequestAlternativeByteHandler,
//                     void *serverRequestAlternativeByteHandlerClientData) override;

    char const *getAuxSDPLine(RTPSink *rtpSink,
                              FramedSource *inputSource) override;

private:
    void setDoneFlag() { fDoneFlag = ~0; }

private:
    char *fAuxSDPLine = nullptr;
    char fDoneFlag = 0; // used when setting up "fAuxSDPLine"
    RTPSink *fDummyRTPSink = nullptr;   // ditto

    std::shared_ptr<Encoder> mEncoder = nullptr;
};


#endif //SCREENCAST_SERVER_H265LIVEVIDEOSERVERMEDIASUBSESSION_H
