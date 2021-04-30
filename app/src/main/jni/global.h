//
// Created by puter on 2020-08-06.
//

#ifndef GST_ANDROID_EXCER_GLOBAL_H
#define GST_ANDROID_EXCER_GLOBAL_H

#include <android/log.h>

#define LOG_TAG "MYTAG"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO , LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN , LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR , LOG_TAG, __VA_ARGS__)


#endif //GST_ANDROID_EXCER_GLOBAL_H
