#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <thread>
#include <cassert>

class ThreadPool{
public : 
    explicit ThreadPool(size_t threadCount = 8) : pool_(std::make_shared<Pool>()) {
        assert(threadCount > 0);
        for(size_t i = 0; i < threadCount; i++) {
            std::thread(
                //thread中执行的函数，可以是函数，函数指针，lambda表示
                //这里定义的是一个lambda表达式，会转为匿名函数执行
                [pool = pool_]{
                    std::unique_lock<std::mutex> locker(pool->mtx);
                    //unique_lock是可以多次解锁上锁的，可以自己解锁
                    while(true) {
                        if(!pool->tasks.empty()) {
                            auto task = std::move(pool->tasks.front());
                            pool->tasks.pop();
                            locker.unlock();
                            task();
                            locker.lock();
                        } else if(pool->isClosed) {
                            break;
                        } else {
                            pool->cond.wait(locker);
                        }
                    }
                }
            ).detach();
            //detach会后台运行，线程完成时，系统会自动回收资源
            //与之对应的是join，join会在前台运行，阻塞当前线程，直到join的线程完成；detach后的线程不能被join
        }
    }

    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;

    ~ThreadPool() {
        if(static_cast<bool>(pool_)){
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                //locker_guard在作用域开始自己上锁，作用域结束自己解锁，不能手动上锁和解锁，作用域就是{}
                pool_->isClosed = true;
            }
            pool_->cond.notify_all();
        }
    }

    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
            //emplace，原地构造对象，避免不必要的赋值或移动，可以提高性能
        }
        pool_->cond.notify_one();
    }

private :
    struct Pool {
        std::mutex mtx;                 //锁    
        std::condition_variable cond;   //条件变量  常用于生产者，消费者模型，cond.wait的时候不会忙等，会释放互斥锁，然后线程进入阻塞态，让出CPU
        bool isClosed;
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<Pool> pool_;
};