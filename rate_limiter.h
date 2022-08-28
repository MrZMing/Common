#pragma once
#include <cstdint>
#include <mutex>
#include <Common/semaphore.h>
namespace common{
namespace ratelimiter {
/*
基于令牌桶实现
给定QPS及最大的爆发区间
内部计算产生一个token需要的us
在Acquire的时候才计算此时可以产生的token，当然有最大的token数限制
通过信号量来控制并发安全，即控制生产token和消费token的安全。
*/

class RateLimiter final 
{
public:
    RateLimiter(int permits_per_sec, double max_burst_seconds = 1);
    ~RateLimiter();
    
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator = (const RateLimiter&) = delete;

    void SetRate(int permits_per_sec);
    //阻塞获取
    void Acquire(int permits);
    //提供一个区间，区间内尽可能多的获取令牌
    void AcquireRange(int min_permits, int max_permits);
    //非阻塞获取
    bool TryAcquire(int permits, int64_t timeout_us = 0);
    bool TryAcquireRange(int min_permits, int max_permits, int64_t timeout_us = 0);

    //强制扣除令牌
    void ForceAcquire(int permits);
    //令牌回收
    void Recharge(int permits);

private:
    void Resync(int64_t now_micros, int64_t max_permits);

    double m_max_burst_us = 0;
    //自旋锁
    //信号量
    double m_stable_interval_us = 0.0;
    int m_permits_per_sec = 0;
    int64_t m_max_permits = 0;
    int64_t m_next_free_us = 0;
    
    std::unique_pre<common::semaphore::Semaphore> m_sem;
    std::mutex m_mu; //线程安全，用mutex互斥访问成员,可优化为自旋锁
};


}
}