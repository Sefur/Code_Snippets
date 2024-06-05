#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <iostream>
#include <thread>

#include <cassert>

class InterruptedSleep
{
public:
    InterruptedSleep() = default;
    ~InterruptedSleep() = default;

    void Sleep(uint64_t ms)
    {
        auto end_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);        
	m_Interp.store(false);

        while (!m_Interp.load()) {

            std::unique_lock<std::mutex> lock(m_Mtx);
            auto status = m_Cond.wait_until(lock, end_time);
            
            /// 一次性等待指定ms时间，直接退出
            if (status == std::cv_status::timeout) {
                std::cout << "wait normal finish" << std::endl;
                break;
            }
            /// 如果被伪唤醒，会继续等待到指定时间点
        }
        /// flag被修改，说明中断生效
        if (m_Interp.load()) {
            std::cout << "wait interrupted" << std::endl;
        }
    }

    void CancelSleep()
    {
        m_Interp.store(true);
        
        /// 通知所有线程，不需要上锁
        m_Cond.notify_all();
    }


private:
    std::mutex m_Mtx;
    std::condition_variable m_Cond;
    std::atomic<bool> m_Interp { false };
};


void TEST_INTERPSLEEP(uint64_t sleep_ms, uint64_t realy_ms)
{
    std::cout <<"[test]----------------------------------\n";
    bool interrupted = sleep_ms != realy_ms ? true : false;

    InterruptedSleep inter_sleep;
    auto ti = std::chrono::high_resolution_clock::now();

    auto t_sleep = std::thread([&inter_sleep, sleep_ms](){
        auto start = std::chrono::high_resolution_clock::now();
        inter_sleep.Sleep(sleep_ms);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "set sleep time:" << sleep_ms << "ms, really sleep time:" << duration.count() << " ms\n" ;
    });

    if (interrupted) {
        std::this_thread::sleep_for(std::chrono::milliseconds(realy_ms));
        inter_sleep.CancelSleep(); 
    }
    t_sleep.join();

    auto to = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(to - ti);
    if (duration.count() == realy_ms) {
        std::cout <<"[  ok]----------------------------------\n";
    }
    else {
        std::cout <<"[fail]----------------------------------\n";
        assert(0);
    }
}


void TEST_MULTI_CALL(uint64_t sleep_ms, int times)
{
    InterruptedSleep inter_sleep;

    int delta = sleep_ms / times;
    int realy_ms = delta;

    for (int i = 0; i < times; ++i) {
    	auto ti = std::chrono::high_resolution_clock::now();

    	auto t_sleep = std::thread([&inter_sleep, sleep_ms](){
       	    auto start = std::chrono::high_resolution_clock::now();
            inter_sleep.Sleep(sleep_ms);
            auto end = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "set sleep time:" << sleep_ms << "ms, really sleep time:" << duration.count() << " ms\n" ;
        });

    	if (realy_ms != sleep_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(realy_ms));
            inter_sleep.CancelSleep(); 
        }
        t_sleep.join();

        auto to = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(to - ti);
        if (duration.count() == realy_ms) {
            std::cout <<"[  ok]----------------------------------\n";
        }
        else {
            std::cout <<"[fail]----------------------------------\n";
            assert(0);
        }

	realy_ms += delta;
    }
}


int main()
{
    TEST_INTERPSLEEP(5000, 1000);
    TEST_INTERPSLEEP(5000, 2000);
    TEST_INTERPSLEEP(5000, 3000);
    TEST_INTERPSLEEP(5000, 4000);
    TEST_INTERPSLEEP(5000, 5000);

    TEST_INTERPSLEEP(5000, 1001);
    TEST_INTERPSLEEP(5000, 2010);
    TEST_INTERPSLEEP(5000, 3100);
    TEST_INTERPSLEEP(5000, 4999);

    TEST_MULTI_CALL(5000, 10); 
    TEST_MULTI_CALL(5000, 20);
    return 0;
}
