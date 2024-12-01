
#include "AndroidUsageEnvironment.h"

#include <string>
#include <android/log.h>

#define LOG_TAG  "live555"

AndroidUsageEnvironment *AndroidUsageEnvironment::createNew(TaskScheduler &taskScheduler) {
    return new AndroidUsageEnvironment(taskScheduler);
}

AndroidUsageEnvironment::AndroidUsageEnvironment(TaskScheduler &taskScheduler)
        : BasicUsageEnvironment0(taskScheduler) {

}

AndroidUsageEnvironment::~AndroidUsageEnvironment() {

}

int AndroidUsageEnvironment::getErrno() const {
    return errno;
}

UsageEnvironment &AndroidUsageEnvironment::operator<<(const char *str) {
    if (str == nullptr) str = "(NULL)"; // sanity check
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", str);
    return *this;
}

UsageEnvironment &AndroidUsageEnvironment::operator<<(int i) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%d", i);
    return *this;
}

UsageEnvironment &AndroidUsageEnvironment::operator<<(unsigned int u) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%u", u);
    return *this;
}

UsageEnvironment &AndroidUsageEnvironment::operator<<(double d) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%f", d);
    return *this;
}

UsageEnvironment &AndroidUsageEnvironment::operator<<(void *p) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%p", p);
    return *this;
}
