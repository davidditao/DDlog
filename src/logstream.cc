#include "logstream.h"

#include <algorithm>
#include <stdint.h>

const char digits[] = "9876543210123456789"; // 保存数字
const char *zero = digits + 9;               // 零所在的位置
const char digitsHex[] = "0123456789ABCDEF"; // 十六进制

// Efficient Integer to String Conversions, by Matthew Wilson.
// 将 int类型 转为 字符串类型 的高效函数
template <class T>
size_t convert(char buf[], T value)
{
    T i = value;
    char *p = buf;

    do
    {
        int lsd = static_cast<int>(i % 10); // 获得个位
        i /= 10;                            // 去除个位
        *p++ = zero[lsd];                   // 得到数字对应的字符
    } while (i != 0);

    if (value < 0)
    {
        *p++ = '-'; // 如果是负数，加上 - 号
    }

    *p = '\0';            // 末尾加上 \0
    std::reverse(buf, p); // 将 \0 之前的字符反转
    return p - buf;       // 返回字符串长度
}

// 将 int类型 转为十六进制的 字符串类型
size_t convertHex(char buf[], uintptr_t value)
{
    uintptr_t i = value;
    char *p = buf;

    do
    {
        int lsd = static_cast<int>(i % 16);
        i /= 16;
        *p++ = digitsHex[lsd];
    } while (i != 0);

    *p = '\0';
    std::reverse(buf, p);
    return p - buf;
}

// 把整型按照T类型格式化到缓冲区中
template <class T>
void LogStream::formatInteger(T v)
{
    if (buffer_.avail() >= kMaxNumericSize)
    {
        size_t len = convert(buffer_.current(), v);
        buffer_.add(len);
    }
}

// 重载 << 运算符
LogStream &LogStream::operator<<(short v)
{
    *this << static_cast<int>(v);
    return *this;
}
LogStream &LogStream::operator<<(unsigned short v)
{
    *this << static_cast<unsigned short>(v);
    return *this;
}
LogStream &LogStream::operator<<(int v)
{
    formatInteger(v);
    return *this;
}
LogStream &LogStream::operator<<(unsigned int v)
{
    formatInteger(v);
    return *this;
}
LogStream &LogStream::operator<<(long v)
{
    formatInteger(v);
    return *this;
}
LogStream &LogStream::operator<<(unsigned long v)
{
    formatInteger(v);
    return *this;
}
LogStream &LogStream::operator<<(long long v)
{
    formatInteger(v);
    return *this;
}
LogStream &LogStream::operator<<(unsigned long long v)
{
    formatInteger(v);
    return *this;
}

LogStream &LogStream::operator<<(const void *p)
{
    // 如果是指针，就转为16进制的形式
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    if (buffer_.avail() > kMaxNumericSize)
    {
        char *buf = buffer_.current();
        buf[0] = '0';
        buf[1] = 'x';
        size_t len = convertHex(buf + 2, v);
        buffer_.add(len + 2);
    }
    return *this;
}
LogStream &LogStream::operator<<(double v)
{
    if (buffer_.avail() > kMaxNumericSize)
    {
        int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12g", v);
        buffer_.add(len);
    }
    return *this;
}
