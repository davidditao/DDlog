#include "logfile.h"

#include <iostream>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

// 写数据到缓冲区
void FileWritter::append(const char *line, const size_t len)
{
    // 将line写入文件file_的缓冲区中，缓冲区的长度可能不足以写完当前数据 
    size_t n = ::fwrite(line, 1, len, file_);
    // 如果没写完要继续写
    size_t remain = len - n;
    while (remain > 0)
    {
        size_t x = ::fwrite(line + n, 1, remain, file_);
        if (x == 0)
        {
            int err = ferror(file_);
            if (err)
            {
                fprintf(stderr, "FileWritter::append() failed %d\n", err);
            }
            break;
        }
        n += x;
        remain -= x;
    }
    written_bytes_ += len;
}

LogFile::LogFile(off_t roll_size)
    : roll_size_(roll_size), // 日志文件的滚动大小
      file_index_(0)
{
    setBaseName();
    rollFile();
}

LogFile::~LogFile() = default;

void LogFile::append(const char *line, const size_t len)
{
    std::unique_lock<std::mutex> guard(mutex_);
    file_->append(line, len);
    if (file_->writtenBytes() > roll_size_)
    {
        rollFile();
    }
}

void LogFile::flush()
{
    std::unique_lock<std::mutex> guard(mutex_);
    file_->flush();
}


// 滚动日志：相当于重新生成日志文件，再向里面写数据 
void LogFile::rollFile()
{
    // 生成一个日志文件名
    std::string file_name = getLogFileNmae();
    // 指向新的文件
    file_.reset(new FileWritter(file_name.c_str()));
    unlink(linkname_);
    symlink(file_name.c_str(), linkname_);
}

void LogFile::setBaseName()
{
    char log_abs_path[PATH_MAX] = {0};
    ::getcwd(log_abs_path, sizeof(log_abs_path));
    strcat(log_abs_path, "/log/");
    if (::access(log_abs_path, 0) == -1)
    {
        ::mkdir(log_abs_path, 0755);
    }

    char process_abs_path[PATH_MAX] = {0};
    long len = ::readlink("/proc/sel/exe", process_abs_path, sizeof(process_abs_path));
    if (len <= 0)
    {
        return;
    }
    char *process_name = strrchr(process_abs_path, '/') + 1;
    snprintf(linkname_, sizeof(linkname_), "%s%s.log", log_abs_path, process_name);
    snprintf(basename_, sizeof(basename_), "%s%s.%d", log_abs_path, process_name, ::getpid());
}

std::string LogFile::getLogFileNmae()
{
    std::string file_name(basename_);
    char timebuf[32] = {0};
    struct tm tm;
    time_t now = time(nullptr);
    ::gmtime_r(&now, &tm);
    strftime(timebuf, sizeof(timebuf), "%Y%m%d-%H%M%S.", &tm);
    file_name += timebuf;

    char index[8] = {0};
    snprintf(index, sizeof(index), "%3d.log", file_index_);
    ++file_index_;
    file_name += index;
    return file_name;
}
