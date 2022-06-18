#pragma once

#include "noncopyable.h"

#include <string>
#include <string.h> // memcpy

const int kSmallBuffer = 4000;        // annotations
const int KLargeBuffer = 4000 * 1000; // annotations

/**
 * 缓冲区类
 * SIZE 为缓冲区大小
 */
template <int SIZE>
class LogBuffer : noncopyable
{
public:
    // 当前位置初始化为缓冲区起始位置
    LogBuffer() : cur_(data_) {}
    ~LogBuffer() {}

    // 末尾添加
    void append(const char *buf, int len)
    {
        // 如果还有空间，将buf加入缓冲区
        if (avail() > len)
        {
            memcpy(cur_, buf, len);
            cur_ += len;
        }
    }

    // 返回缓冲区头指针
    const char *data() const { return data_; }
    // 返回已使用长度
    int length() const { return static_cast<int>(cur - data_); }
    // 当前指针向后移动 len 个位置
    void add(size_t len) { cur_ += len; }
    // 返回当前位置指针
    char *current() { return cur_; }
    // 重置缓冲区
    void reset() { cur_ = data_; }
    // 将缓冲区置空
    void bzero() { memset(); }
    // 返回缓冲区剩余大小
    int avail() const { return static_cast<int>(end() - cur_); }

private:
    // 返回缓冲区末尾指针
    const char *end() const { return data_ + sizeof(data_); }

    char data_[SIZE]; // 缓冲区
    char *cur_;       // 指向当前位置指针
};

/**
 * 流式化输出日志类
 */
class LogStream : noncopyable
{
    using self = LogStream;

public:
    using Buffer = LogBuffer<kSmallBuffer>;

    // 重载 << 运算符
    self &operator<<(bool v)
    {
        buffer_.append(v ? "1" : "0", 1);
        return *this;
    }
    self &operator<<(short);
    self &operator<<(unsigned short);
    self &operator<<(int);
    self &operator<<(unsigned int);
    self &operator<<(long);
    self &operator<<(unsigned long);
    self &operator<<(long long);
    self &operator<<(unsigned long long);
    self &operator<<(const void *);
    self &operator<<(float v)
    {
        *this << static_cast<double>(v);
        return *this;
    }
    self &operator<<(double);
    self &operator<<(char v)
    {
        // 如果是字符，直接添加到缓冲区中
        buffer_.append(&v, 1);
        return *this;
    }
    self &operator<<(const char *str)
    {
        if (str)
        {
            // 如果是字符串且不为空，直接添加到缓冲区中
            buffer_.append(str, static_cast<int>(strlen(str)));
        }
        else
        {
            // 如果为空，输出 (null)
            buffer_.append("(null)", 6);
        }
    }
    self &operator<<(const unsigned char *v) { return operator<<(reinterpret_cast<const char *>(v)); }
    self &operator<<(const std::string &v)
    {
        buffer_.append(v.c_str(), static_cast<int>(v.size()));
        return *this;
    }
    self &operator<<(const Buffer &v)
    {
        buffer_.append(v.data(), v.length());
        return *this;
    }

    // 向缓冲区后面添加
    void append(const char *data, int len) { buffer_.append(data, len); }
    // 返回缓冲区
    const Buffer &buffer() { return buffer_; }
    // 重置缓冲区
    void resetBuffer() { buffer_.reset(); }

private:
    // 把整型按照T类型格式化到缓冲区中
    template <class T>
    void formatInteger(T);

    Buffer buffer_;                        // 缓冲区
    static const int kMaxNumericSize = 32; // annotations
};
