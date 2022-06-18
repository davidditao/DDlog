#pragma once

#include "noncopyable.h"

#include <memory>
#include <mutex>
#include <stdio.h>
#include <limits.h>

// 用于写日志数据到本地文件
class FileWritter : noncopyable
{
public:
    explicit FileWritter(std::string file_name)
        : file_(::fopen(file_name.c_str(), "ae")), // 'e' for O_CLOEXEC
          written_bytes_(0)
    {
        // <stdio.h> 设置文件流的缓冲区
        ::setbuffer(file_, buffer_, sizeof(buffer_));
    }

    ~FileWritter() { ::fclose(file_); } // 关闭文件，会强制flush缓冲区

    // 返回已经写入日志的字节数
    off_t writtenBytes() const { return written_bytes_; }
    // 写数据到缓冲区
    void append(const char *line, const size_t len);
    // 刷新缓冲区数据到文件
    void flush() { ::fflush(file_); }

private:
    FILE *file_;             // 文件指针
    char buffer_[64 * 1024]; // 文件输出缓冲区, 64kB
    off_t written_bytes_;    // 已经写入日志的字节数
};

// annotation
class LogFile : noncopyable
{
public:
    LogFile(off_t roll_size);
    ~LogFile();

    void append(const char *line, const size_t len);
    void flush();
    // 滚动日志
    void rollFile();

private:
    void setBaseName();
    std::string getLogFileNmae();

    char linkname_[PATH_MAX];
    char basename_[PATH_MAX];
    off_t roll_size_;
    int file_index_;
    std::mutex mutex_;
    std::unique_ptr<FileWritter> file_;
};