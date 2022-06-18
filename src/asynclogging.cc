#include "asynclogging.h"
#include "logfile.h"

#include <iostream>
#include <unistd.h>
#include <memory>
#include <stdio.h>
#include <functional>

AsyncLogging::AsyncLogging(int flush_interval, int roll_size)
    : flush_interval_(flush_interval),
      roll_size_(roll_size),
      running_(true),
      thread_(std::bind(AsyncLogging::writeThread, this), "AsyncLoggingThread"),
      current_buffer_(new Buffer),
      next_buffer_(new Buffer),
      buffers_()
{
    current_buffer_->bzero();
    next_buffer_->bzero();
    buffers_.reserve(8);
}

// 所有的LOG_ 最终都会调用 AsyncLogging::append
void AsyncLogging::append(const char *buf, int len)
{
    // 加锁
    std::unique_lock<std::mutex> guard(mutex_);
    // 如果当前Buffer还有空间，就添加到当前日志
    if (current_buffer_->avail())
    {
        current_buffer_->append(buf, len);
    }
    else // 如果当前Buffer已满，需要通知日志线程有数据可写
    {
        // 把当前Buffer 添加到列表中
        buffers_.push_back(std::move(current_buffer_));
        // 将下一个Buffer 设置为当前 Buffer
        if (next_buffer_)
        {
            current_buffer_ = std::move(next_buffer_);
        }
        else
        {
            // 如果写入速度太快，两个缓冲区都满了，那么分配一块新的Buffer
            current_buffer_.reset(new Buffer); // 极少发生
        }
        // 更换完Buffer 后，再将数据写入
        current_buffer_->append(buf, len);
        // 通知日志线程，有数据可写(只有当缓冲区满了才将日志写入文件)
        cond_.notify_one();
    }
}

// 异步日志线程
void AsyncLogging::writeThread()
{
    // 创建两个Buffer
    BufferPtr new_buffer1(new Buffer);
    BufferPtr new_buffer2(new Buffer);
    new_buffer1->bzero();
    new_buffer2->bzero();
    // Buffer列表
    BufferVector buffers_to_write;
    buffers_to_write.reserve(8);

    LogFile output(roll_size_);

    while (running_)
    {
        { // 锁的临界区
            // 加锁
            std::unique_lock<std::mutex> guard(mutex_);
            if (buffers_.empty())
            {

                // 如果没人唤醒，等待指定时间
                cond_.wait_for(guard, std::chrono::milliseconds(flush_interval_));
            }

            // 这里还需要将 current_buffer_ 放入列表中
            buffers_.push_back(std::move(current_buffer_));
            // 将new_buffer1 设为当前缓冲区
            current_buffer_ = std::move(new_buffer1);
            // 转移buffers_
            buffers_to_write.swap(buffers_);
            if (!next_buffer_)
            {
                // 如果 next_buffer_ 也被使用，需要将它设置为 new_buffer2
                next_buffer_ = std::move(new_buffer2);
            }
        }

        if (buffers_to_write.size() > 16)
        {
            char buf[256];
            snprintf(buf, sizeof(buf), "Dropped log messages %zd larger buffers\n", buffers_to_write.size() - 2);
            fputs(buf, stderr);
            // 如果日志太多，丢掉多余的，只留两个缓冲区
            buffers_to_write.erase(buffers_to_write.begin() + 2, buffers_to_write.end());
        }

        // 将列表中的日志入到文件中
        for (const auto &buffer : buffers_to_write)
        {
            output.append(buffer->data(), static_cast<size_t>(buffer->length()));
        }

        // 写完后调整列表的大小
        if (buffers_to_write.size() > 2)
        {
            buffers_to_write.resize(2);
        }

        if (!new_buffer1)
        {
            // 从 buffers_to_write中弹出一个作为newBUffer1
            new_buffer1 = std::move(buffers_to_write.back());
            buffers_to_write.pop_back();
            // 清理 newBuffer1
            new_buffer1->reset();
        }

        if (!new_buffer2)
        {
            new_buffer2 = std::move(buffers_to_write.back());
            buffers_to_write.pop_back();
            new_buffer2->reset();
        }
        buffers_to_write.clear();
        output.flush();
    }
    output.flush();
}
