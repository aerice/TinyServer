#program once

#include <thread>
#include <vector>
#include <function>
#include <atomic>
#include <future>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <algorithm>
#include <stdexcept>

class ThreadGuard
{
  private:
    std::vector<std::thread>& threads;
  private:
    ThreadGuard(const ThreadGuard&) = delete;
    ThreadGuard& operator= (const ThreadGuard&) = delete;
    ThreadGuard(ThreadGuard&&) = delete;
    ThreadGuard& operator= (ThreadGuard&&) = delete; 
  public:

    ThreadGuard(std::vector<std::thread>& ts)
      : threads(ts)
    {
    }

    ~ThreadGuard()
    {
      for(int i = 0; i != threads.size(); ++i)
      {
        if( threads[i].joinable() )
        {
          threads[i].join();
        }
      }
    }
};

class ThreadPool 
{
  public:
    typedef std::function<void()> task_type;

    explicit ThreadPool(int size = 4);
    ~ThreadPool();

    void stop();
    
    template<typename F, typename... Args>
    auto add(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

  private:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    void ThreadPool(const ThreadPool&) = delete;
    void ThreadPool(ThreadPool&&) = delete;

  private:
    // uint32_t m_ThreadNumber;
    // uint32_t m_MaxRequests;
    std::vector<std::vector> m_Threads;
    std::queue<task_type> m_Tasks;
    ThreadGuard m_ThreadGurad;

    std::mutex m_mt;
    std::condition_variable m_cond;
    std::atomic<bool> m_stop;  // 线程池是否停止

    //
};


ThreadPool::ThreadPool(int size)
  : m_stop(false),
    m_ThreadGurad(m_Threads)
  {
    int nThreads = size;
    if(nThreads < 0) {
      nThreads = std::thread::hardware_concurrency();
      nThreads = ( nThreads == 0 ? 2 : nThreads);
    }

    for(int i = 0; i < nThreads; ++i)
    {
      m_Threads.emplace_back(
        std::thread( [this](){
          while(!m_stop) {
            task_type task;
            {
              std::unique_lock<std::mutex> lock(this->m_mt);
              this->m_cond.wait(lock, [this]{return m_stop || !m_Tasks.empty();}) // wait直到有task
              if( m_stop && m_Tasks.empty() ) 
                return;
              task = std::move(m_Tasks.front());
              task.pop();
            }
            task(); // 执行任务
          }
        } ) );
    }
  }


/*
提交一个任务
调用.get方法获取返回值会等待任务执行完，获取返回值
*/
template<typename F, typename... Args>
ThreadPool::add(F&& f, Args&&... args) -> std::future<decltype(f(args...))
{
  if(m_stop) 
    throw std::runtime_error("ThreadPool is stopped.");
  
  using return_type = decltype(f(args...)); // 函数f的返回值类型
  using packaged_task = std::packaged_task<return_type()>;

  auto task = std::make_shared<packaged_task>(
    std::bind(std::forward<F>(f), std::forward<Args>(args)...
  ); // 把函数入口及参数，绑定

  std::future<return_type> future = task->get_future();
  { //添加任务到新队列
    std::lock_guard<std::mutex> lock(m_mt);
    m_Tasks.emplace( [task](){
      (*task)();
    } );
  }
  m_cond.notify_one();
  return future;
}


void ThreadPool::stop()
{
  m_stop = true;
}

ThreadPool::~ThreadPool()
{
  stop();
  m_cond.notify_all();
}

