#include "timestamp.h"
#include <sys/time.h>

/**
 * struct timeval{
 *      long tv_sec;  // Seconds
 *      long tv_usec; // Microseconds
 * };
 *
 * // 该函数会将时间包装到结构体 timeval 里，将时区信息包装到 timezone 里
 * int gettimeofday(struct timeval *tv, struct timezone *tz);
 */

int64_t Timestamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    // 返回Epoch(1970-1-1)到当前时间经过了多少微秒
    return tv.tv_sec * kMicroSecondsPerSecond + tv.tv_usec;
}
