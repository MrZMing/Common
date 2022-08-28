#pragma once

#include <mutex>
#include <condition_variable>

namespace common{
namespace semaphore{

class Semaphore
{
public:
    Semaphore(int value = 0) : m_value(value) {}
    ~Semaphore() = default;

    void Acquire(int cnt = 1)
    {
        //获取
        if (cnt < 0) return;
        std::unique_lock<std::mutex> lock(m_mu);
        //阻塞等待直到条件成立
        m_cond.wait(lock, [this, cnt]
        {
            return m_value >= cnt;
        });
        m_value -= cnt;
    }
    
    bool TryAcquire(int cnt = 1)
    {
        if (m_value < cnt) return false;
        std::unique_lock<std::mutex> lock(m_mu);
        if (m_value < cnt) return false; //double check
        m_value -= cnt;
        return true;
    }

    bool TimedAcquire(int time_ms = 0, int cnt = 1)
    {
        if (time_ms == 0)
        {
            return TryAcquire(cnt);
        }
        std::unique_lock<std::mutex> lock(m_mu);
        if(
            m_cond.wait_for(lock, time_ms, [this, cnt]()
            {
                return m_value >= cnt;
            }))
        {
            m_value -= cnt;
            return true;
        }
        else 
        {
            return false;
        }
    }

    void Release(int cnt = 1)
    {
        std::unique_lock<std::mutex> lock(m_mu);
        m_value += cnt;
        if (m_value > 1)
        {
            m_cond.notify_all();
        }
        else
        {
            m_cond.notify_one();
        }
    }

    int GetValue() const
    {
        return m_value;
    }

    void SetValue(int cnt = 1)
    {
        std::unique_lock<std::mutex> lock(m_mu);
        m_value = cnt;
    }

    void DecreaseValue(int cnt)
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_value -= cnt;
    }
    
private:
    int m_value;
    std::condition_variable m_cond;
    std::mutex m_mu;
};

}
}