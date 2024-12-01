
#ifndef SCREENCAST_SERVER_ANDROIDUSAGEENVIRONMENT_H
#define SCREENCAST_SERVER_ANDROIDUSAGEENVIRONMENT_H

#include "BasicUsageEnvironment0.hh"

class AndroidUsageEnvironment : public BasicUsageEnvironment0 {
public:
    static AndroidUsageEnvironment* createNew(TaskScheduler& taskScheduler);

    // redefined virtual functions:
    virtual int getErrno() const;

    virtual UsageEnvironment& operator<<(char const* str);
    virtual UsageEnvironment& operator<<(int i);
    virtual UsageEnvironment& operator<<(unsigned u);
    virtual UsageEnvironment& operator<<(double d);
    virtual UsageEnvironment& operator<<(void* p);

protected:
    AndroidUsageEnvironment(TaskScheduler& taskScheduler);
    // called only by "createNew()" (or subclass constructors)
    virtual ~AndroidUsageEnvironment();
};


#endif //SCREENCAST_SERVER_ANDROIDUSAGEENVIRONMENT_H
