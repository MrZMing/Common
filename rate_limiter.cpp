#include <rate_limiter.h>
#include <iostream>
#include <chrono>

using namespace std;

namespace common{
namespace ratelimiter{

int64_t GetNowMicros()
{
    // 获取当前的时间ns然后转换成us
    int64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>( 
                        std::chrono::steady_clock::now().time_since_epoch()).count();
    return now_us;
}

RateLimiter::~RateLimiter()
{
    
}


//设置QPS多少，即一秒内能获取到多少token
RateLimiter::RateLimiter(int permits_per_sec, double max_burst_seconds)
    :m_max_burst_us(max_burst_seconds * 1E6) //秒转us
{
    if (permits_per_sec <= 0)
    {
        permits_per_sec = 1;
    }
    if (max_burst_seconds <= 0)
    {
        m_max_burst_us = 1;
    }
    //计算下次释放的us 
    m_next_free_us = GetNowMicros() + m_max_burst_us;
    SetRate(permits_per_sec);
}

void RateLimiter::SetRate(int permits_per_sec)
{
    //重新设置速度
    if (permits_per_sec == m_permits_per_sec)
    {
        return; //一样就不需要设置了
    }
    if (permits_per_sec <= 0)
    {
        permits_per_sec = 1;
    }
    if (permits_per_sec > 1E6)
    {
        //1s = 1E6 us，最多就是1E6个
        permits_per_sec = 1E6;
    }
    std::unique_lock<std::mutex> lock(m_mu);
    m_permits_per_sec = permits_per_sec;
    m_stable_interval_us = (double)1E6 / permits_per_sec; //计算出1个token的us区间
    int64_t old_max_permits = m_max_permits;
    //通过最大的爆发区间，计算出瞬时可获取的最大token
    m_max_permits = std::max(1L, int64_t(m_max_burst_us / 1E6 * permits_per_sec));
    if (old_max_permits == 0)
    {
        //生成信号量
        m_sem->reset(new common::semaphore::Semaphore(m_max_permits));
    }
    else if (m_max_permits > old_max_permits)
    {
        //增加多余的部分
        m_sem->Release(m_max_permits - old_max_permits);
    }
    else
    {
        //强制去掉剩余部分
        m_sem->Decrease(old_max_permits - m_max_permits);
    }
}

void RateLimiter::Acquire(int permits)
{
    AcquireRange(permits, permits);
}

// int RateLimiter::AcquireRange(int min_permits, int max_permits)
// {
//     if (min_permits <= 0 || max_permits <= 0)
//     {
//         return 0;
//     }
//     int permits = max_permits;
//     while(TryAcquireRange(permits, permits, m_max_burst_us / 5) <= 0)
//     {
//         permits = std::max(min_permits, )
//     }
// }

int RateLimiter::TryAcquire(int permits, int timeout_ms)
{
    return TryAcquireRange(permits, permits, timeout_ms);
}

int RateLimiter::TryAcquireRange(int min_permits, int max_permits, int timeout_us)
{
    if (min_permits <= 0 || max_permits <= 0)
    {
        return 0;
    }
    if (timeout_ms < 0)
    {
        timeout_ms = 0;
    }
    //获取前先释放,释放有最大值限制，
    Resync(GetNowMicros(), std::max(int64_t(max_permits), m_max_permits));
    int permits = std::max(min_permits, std::min(max_permits, int(m_sem->GetValue())));
    if (m_sem->TimedAcquire(timeout_us / 1000, permits))
    {
        return permits;
    }
    return 0;
}

void RateLimiter::ForceAcquire(int permits)
{
    m_sem->DecreaseValue(permits);
}

void RateLimiter::Recharge(int permits)
{
    m_sem->Release(permits);
}

void RateLimiter::Resync(int64_t now_us, int64_t max_permits)
{
    std::unique_lock<std::mutex> lock(m_mu);
    if (now_us > m_next_free_us)
    {
        double new_permits = double(now_us - m_next_free_us) / m_stable_interval_us;
        int64_t stored_permits = m_sem->GetValue();
        double release_permits;
        if (max_permits > stored_permits + new_permits)
        {
            release_permits = new_permits;
        }
        else 
        {
            release_permits = (double)max_permits - stored_permits;
        }
        if (release_permits >= 1)
        {
            m_sem->Release(release_permits);
            //向左偏移？
            m_next_free_us = now_us - (release_permits - int64_t(release_permits)) * m_stable_interval_us;
        }
    }
}



}
}