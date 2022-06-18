#pragma once
#include <string.h>
#include <functional>

#include "logstream.h"
#include "asynclogging.h"

class Logger
{
public:
    // 日志级别
    enum LogLevel
    {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVELS,
    };

    // 内部类：文件相关操作
    class SourceFile
    {
    public:
        SourceFile(const char *file) : file_(file)
        {
            // 搜索file中最后一个 / 后的字符
            const char *slash = strrchr(file, '/');
            if (slash)
            {
                file = slash + 1;
            }
            size_ = static_cast<int>(strlen(file));
        }
        const char *file_; // 文件名
        int size_;         // 文件大小
    };

    Logger(SourceFile file, int line);
    Logger(SourceFile file, int line, LogLevel level);
    Logger(SourceFile file, int line, LogLevel level, const char *func_name);

    ~Logger();

    // annotations
    LogStream &stream() { return impl_.stream_; }
    // 返回当前日志级别
    static LogLevel logLevel();
    // 设置当前日志级别
    static void setLogLevel(LogLevel level);
    // annotations
    static void setAsync();

    // 输出方法回调
    using OutputFunc = std::function<void(const LogStream::Buffer &)>;
    // 设置输出方法
    static void setOutputFunc(OutputFunc func);

    // 内部类: 日志消息的格式
    class Impl
    {
    public:
        using LogLevel = Logger::LogLevel;
        Impl(LogLevel level, const SourceFile &file, int line);

        // 格式化时间
        void forMatTime();
        // 获取当前线程id
        void getThreadId();

        int64_t time_;     // 当前时间
        LogStream stream_; // 输出流(其中有一个缓冲区)
        LogLevel level_;   // 日志等级
        SourceFile file_;  // 文件名
        int line_;         // 当前行
    };

private:
    Impl impl_;            // annotations
    static bool is_async_; // annotations
};

// 全局变量：当前日志级别
extern Logger::LogLevel g_log_level;
// 返回当前日志级别
inline Logger::LogLevel Logger::logLevel()
{
    return g_log_level;
}

// 全局变量：annotations
extern bool g_is_async_;
// annotations
inline void Logger::setAsync()
{
    g_is_async_ = true;
}

/**
 * 用户调用的宏
 */
// 设置当前日志级别
#define SET_LOGLEVEL(x) Logger::setLogLevel(x);

// annotations
#define LOG_SET_ASYNC(x)                                                                       \
    if (x != 0)                                                                                \
    {                                                                                          \
        static AsyncLogging g_async_;                                                          \
        Logger::setOutputFunc(                                                                 \
            [&](const LogStream::Buffer &buf) { g_async_.append(buf.data(), buf.length()); }); \
        Logger::setAsync();                                                                    \
    }

#define LOG_TRACE                            \
    if (Logger::logLevel() <= Logger::TRACE) \
    (Logger(__FILE__, __LINE__, Logger::TRACE, __func__).stream())

#define LOG_DEBUG                            \
    if (Logger::logLevel() <= Logger::DEBUG) \
    (Logger(__FILE__, __LINE__, Logger::DEBUG, __func__).stream())

#define LOG_INFO                            \
    if (Logger::logLevel() <= Logger::INFO) \
    (Logger(__FILE__, __LINE__, Logger::INFO, __func__).stream())

#define LOG_WARN logger(__FILE__, __LINE__, Logger::WARN, __func__).stream()
#define LOG_ERROR logger(__FILE__, __LINE__, Logger::ERROR, __func__).stream()
#define LOG_FATAL logger(__FILE__, __LINE__, Logger::FATAL, __func__).stream()