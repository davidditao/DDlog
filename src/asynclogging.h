#pragma once

#include "logstream.h"
#include "noncopyable.h"

#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

class AsyncLogging : noncopyable
{
    using Buffer = LogBuffer<KLargeBuffer>;
    using BufferVector = std::vector<std::unique_ptr<Buffer>>;
    using BufferPtr = BufferVector::value_type;

public:
    AsyncLogging(int flush_interval = 500, int roll_size = 20 * 1024 * 1024);
    ~AsyncLogging()
    {
        if (running_)
        {
            stop();
        }
    }

    void append(const char *buf, int len);

    void stop()
    {
        running_ = false;
        thread_.join();
    }

private:
    void writeThread();

    const int flush_interval_;  // 定时缓冲时间
    const int roll_size_;       //
    std::atomic<bool> running_; // 是否正在运行
    std::thread thread_;        // 执行改异步日志记录器的线程

    std::mutex mutex_;
    std::condition_variable cond_;

    BufferPtr current_buffer_; // 当前缓冲区
    BufferPtr next_buffer_;    // 预备缓冲区
    BufferVector buffers_;     // 缓冲区队列：待写入文件
};