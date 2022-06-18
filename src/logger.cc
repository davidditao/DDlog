#include "logger.h"
#include "timestamp.h"

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

// 保存日志级别的名字
const char *LogLevelName[Logger::NUM_LOG_LEVELS] = {
    "TRACE ",
    "DEBUG ",
    "INFO ",
    "WARN ",
    "ERROR ",
    "FATAL ",
};

// 设置当前日志级别
void Logger::setLogLevel(LogLevel level)
{
    g_log_level = level;
}

// 全局变量：当前日志级别, 默认为 INFO
Logger::LogLevel g_log_level = Logger::INFO;

// 全局变量：是否使用异步日志，默认为false
bool g_is_async_ = false;

// 该类方便存储字符串长度信息
class T
{
public:
    T(const char *str, unsigned len) : str_(str), len_(len) {}
    const char *str_;
    const int len_;
};

// 重载输入 T 类型时的 << 运算符
inline LogStream &operator<<(LogStream &s, T v)
{
    s.append(v.str_, v.len_);
    return s;
}
// 重载输入 SourceFile 类型时的 << 运算符
inline LogStream &operator<<(LogStream &s, Logger::SourceFile v)
{
    s.append(v.file_, v.size_);
    return s;
}

// 默认输出到stdout
void defaultOutput(const LogStream::Buffer &buf)
{
    // fwrite 是线程安全的！
    size_t n = fwrite(buf.data(), 1, static_cast<size_t>(buf.length()), stdout);
    // FIXME check n
    (void)n;
}

// 全局变量：输出方法回调, 初始化为默认输出
Logger::OutputFunc g_output_func = defaultOutput;
// 设置输出方法
void Logger::setOutputFunc(OutputFunc func)
{
    g_output_func = func;
}

Logger::Logger(SourceFile file, int line)
    : impl_(INFO, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level)
    : impl_(level, file, line)
{
}
Logger::Logger(SourceFile file, int line, LogLevel level, const char *func_name)
    : impl_(level, file, line)
{
    impl_.stream_ << func_name << ' ';
}

// Logger对象析构的时候将缓冲区中的内容输出
Logger::~Logger()
{
    // 将换行符写入缓冲区中
    stream() << "\n";

    const LogStream::Buffer &buf(stream().buffer());
    // 将缓冲区中的所有内容输出。默认输出到stdout
    g_output_func(buf);
}

// Impl对象构造时就将日志的消息格式拼接后写入缓冲区中
Logger::Impl::Impl(LogLevel level, const SourceFile &file, int line)
    : time_(Timestamp::now()),
      stream_(),
      level_(level),
      file_(file),
      line_(line)
{
    forMatTime();
    getThreadId();
    stream_ << T(LogLevelName[level], 6);
    stream_ << file_ << ':' << line_ << "->";
}

// 格式化时间
void Logger::Impl::forMatTime()
{
    time_t seconds = static_cast<time_t>(time_ / Timestamp::kMicroSecondsPerSecond);       // 秒
    time_t micro_seconds = static_cast<time_t>(time_ % Timestamp::kMicroSecondsPerSecond); // 毫秒

    struct tm tm_time;
    // 获取UTC格式时间，线程安全
    ::gmtime_r(&seconds, &tm_time);

    // 拼接时间
    char t_time[64] = {0};
    snprintf(t_time, sizeof(t_time), "%4d-%02d-%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour + 8, tm_time.tm_min, tm_time.tm_sec);
    // 拼接毫秒
    char buf[32] = {0};
    snprintf(buf, sizeof(buf), ".%03d", micro_seconds);
    // 输出
    stream_ << T(t_time, 19) << T(buf, 4);
}

// 获取当前线程id
void Logger::Impl::getThreadId()
{
    char buf[32] = {0};
    int length = snprintf(buf, sizeof(buf), "%5lu", ::syscall(SYS_gettid));
    stream_ << T(buf, length);
}
