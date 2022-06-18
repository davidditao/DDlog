# mylog

基于C++的高性能异步日志库。

1. 支持多级别日志消息，并且日志的输出级别在运行时可调。
2. 支持多线程程序并发写日志到一个日志文件中。
3. 支持日志文件的滚动。
4. 日志库前端使用 C++ 的 stream << 风格。

## 1. 多线程程序中的日志系统如何保证线程安全？

1. 用一个全局的互斥锁：会造成全部线程抢占一个锁，效率低下。
2. 每个线程单独写一个日志文件：有可能让业务线程阻塞在写磁盘操作上。

解决办法：用一个后端线程负责收集日志消息没，并写入日志文件，前端线程只负责往后端先程中发送日志消息。这就是 **异步日志**



## 2. 为什么需要异步日志？

在多线程程序，异步日志是必须的。因为如果在网络IO线程或业务线程中直接往磁盘写数据的话，写操作偶尔可能阻塞长达数秒之久（可能是磁盘或磁盘控制器复位）。这可能导致请求方超时，或者耽误发送心跳消息。所以在正常的业务处理流程中应该避免磁盘IO，尤其是在 one loop per thread 模型中，因为此时线程是复用的，阻塞线程意味着影响多个客户连接。



## 3. 如何实现日志文件的滚动？

如果要将日志写入文件中，那么日志文件的滚动是必须的，这样可以简化日志归档的实现。

日志文件滚动的条件有两个：**文件大小** 和 **时间**。

日志文件在大于 1GB 的时候会更换新的文件，或者每隔一天会更换新的文件。



## 4. 如何实现异步日志？- 双缓冲技术

**双缓冲技术** 的基本思路是准备块 Buffer：A 和 B，前端负责往 A 填数据（日志消息），后端负责将 B 的数据写入文件。当 A 写满后，交换 A 和 B，让后端将 A 的数据写入文件，而前段则往 B 中填入新的数据，如此往复。

使用两个 Buffer 的好处是，在前端写入日志消息的时候，不需要等待磁盘文件操作，也避免了每条新日志消息都唤醒后端日志线程。换言之，前端不是将一条条日志消息分别发送给后端，而是将多条消息拼成一个大的 Buffer 传送给后端，相当于批处理，减少了线程唤醒的频度，降低开销。

此外为了防止程序崩溃时各个线程来不及将日志写入磁盘，日志库会定期将缓冲区内的日志消息刷新到磁盘中。



## 5. 关键代码

### 同步日志

`logger.h`  给出了供用户调用的宏：

```c++
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
```

我们输出日志的时候使用 `LOG_XXX<<` 后面加上日志消息。宏定义中实际上是创建了一个 **Logger 的匿名对象**，并调用这个匿名对象的 *Logger::stream() 方法*。这个方法会返回一个 `LogStream` 对象， `LogStream` 重载了*<<* 运算符，可以将日志消息存入 `LogStream` 对象的 `Buffer` 中。

**为什么要使用匿名对象呢？** 在 LOG 语句结束的时候，匿名对象就会马上被销毁，因此会调用析构方法 *~Logger()* ，在析构方法中会将缓冲区中所有的内容输出到后端。

```C++
// Logger对象析构的时候将缓冲区中的内容输出
Logger::~Logger()
{
    // 将换行符写入缓冲区中
    stream() << "\n";

    const LogStream::Buffer &buf(stream().buffer());
    // 将缓冲区中的所有内容输出。默认输出到stdout
    g_output_func(buf);
}
```

这种方法很巧妙地实现了对象生命周期的管理。



### 异步日志

我们可以用如下语句将日志设置为异步。

```C++
// set to asynchronous logger
LOG_SET_ASYNC(1)
```

实际的实现使用了四个缓冲区（前端两个，后端两个），这样可以进一步减少或避免日志前端的等待。

数据结构如下：

```C++
// asynclogging.h
using Buffer = LogBuffer<KLargeBuffer>;
using BufferVector = std::vector<std::unique_ptr<Buffer>>;
using BufferPtr = BufferVector::value_type;   
// 前端的两个缓冲区
BufferPtr current_buffer_; // 当前缓冲区
BufferPtr next_buffer_;    // 预备缓冲区
BufferVector buffers_;     // 缓冲区队列：待写入文件
```

在日志设置为异步后，前端会将回调函数 *g_output_func()* 设置为下面这个函数：

```C++
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
```

因为前端可能有多个线程会同时调用这个输出的回调函数，所以我们需要对这段代码加上互斥锁。接下来的操作分为两种情况：

+ 当前缓冲区还有足够空间时，将日志消息直接添加到当前缓冲区中。

+ 否则，将当前缓冲区添加到就绪队列 `buffers_` 中，并将预备缓冲区设置为当前缓冲区，然后将日志消息写入。最后通知后端的日志线程，开始将已满的缓冲区中的数据写入磁盘。

以上这两种情况在临界区内都没有耗时操作。第一种情况中 *append()* 方法只调用了 *memcpy()* 函数。而第二种情况使用了 **移动语义** 代替了复制，速度也是非常快的。

再来看看后端日志线程的实现：

```C++
// 异步日志线程
void AsyncLogging::writeThread()
{
    // 创建两个Buffer
    BufferPtr new_buffer1(new Buffer);
    BufferPtr new_buffer2(new Buffer);
    // Buffer列表
    BufferVector buffers_to_write;
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
                // 将 next_buffer_ 设置为 new_buffer2：这样前端始终有一个预备的buffer可以使用
                next_buffer_ = std::move(new_buffer2);
            }
        } // 退出临界区
		// 将队列中的日志入到文件中
        // 写完后重置缓冲区
    }
    // flush output
}

```

后端也有两块 Buffer。在临界区中，条件变量唤醒的条件有两个：一是超时，二是前端写满了至少一个 Buffer。

当条件满足时，先将当前缓冲区（*currentBuffer_*）移入 *buffers_*，并立刻将空闲的 *newBuffer1* 设置为当前缓冲区。

>  注意这里加的锁还是 *mutex_*，所以对缓冲区的操作不会出现竞争。

接下来将 *buffers_* 与 *buffers_to_write* 交换，后面的代码就可以在临界区外安全地访问 *buffers_to_write* 了。

最后还需要将 *next_buffer_* 设置为 *new_buffer2*，这样前端始终有一个预备的buffer可以使用。

后端的代码在临界区内也没有耗时的操作（没有复制，用的都是移动）。

## 6. 运行图示



![logwrite](README.assets/logwrite.png)













