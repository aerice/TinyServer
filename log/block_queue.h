
//  采用循环数组的方式实现阻塞队列
// 采用加锁保证线程安全

#progma once

#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
using namespace std;

template<typename T>
class block_queue
{
public:
    block_queue(int max_size = 1000);
    ~block_queue();

    void clear();
    bool full() const;
    bool empty() const;
    bool front(T& value) const;
    bool back(T& value) const;
    int size() cosnt;
    int maxSize() cosnt;

    bool push(const T& item);
    bool pop(T& item);
    bool pop(T& item, int timeout); // 增加超时处理

private:
    pthread_mutex_t* m_mutex;
    pthread_cond_t* m_cond;
    T* m_array;
    int m_size;
    int m_maxSize;
    int m_front;
    int m_back;
};


// 

template<typename T>
block_queue<T>::block_queue(int max_size)
    : m_size(0), m_front(-1), m_back(-1)
{
    if (max_size <= 0)
    {
        exit(-1);
    }

    m_maxSize = max_size;
    m_array = new T[max_size];
    // 创建锁和条件变量
    m_mutex = new pthread_mutex_t;
    m_cond = new pthread_cond_t;
    pthread_mutex_init(m_mutex, NULL);
    pthread_cond_init(m_cond, NULL);
}

template<typename T>
block_queue<T>::~block_queue()
{
    pthread_mutex_lock(m_mutex);
    if(m_array != NULL)
        delete[] m_array;  // 注意申请的内存
    pthread_mutex_unlock(m_mutex);

    pthread_mutex_destroy(m_mutex);
    pthread_cond_destroy(m_cond);
    delete m_mutex;
    delete m_cond;
}

template<typename T>
void block_queue<T>::clear()
{
    pthread_mutex_lock(m_mutex);
    m_size = 0;
    m_front = -1;
    m_back = -1;
    pthread_mutex_unlock(m_mutex);
}

template<typename T>
bool block_queue<T>::full() const
{
    pthread_mutex_lock(m_mutex);
    if(m_size >= m_maxSize)
    {
        pthread_mutex_unlock(m_mutex);
        return true;
    }
    pthread_mutex_unlock(m_mutex);
    return false;
}

template <typename T>
bool block_queue<T>::empty() const
{
    pthread_mutex_lock(m_mutex);
    if(m_size == 0){
        pthread_mutex_unlock(m_mutex);
        return true;
    }
    pthread_mutex_unlock(m_mutex);
    return false;
}

// 返回队首元素
template<typename T>
bool block_queue<T>::front(T& value) const
{
    pthread_mutex_lock(m_mutex);
    if(m_size == 0){
        pthread_mutex_unlock(m_mutex);
        return false;
    }
    value = m_array[m_front+1 % m_maxSize]; // 这里因为m_front逻辑上指向的是队首的前一个位置
    pthread_mutex_unlock(m_mutex);
    return true;
}

template<typename T>
bool block_queue<T>::front(T& value) const
{
    pthread_mutex_lock(m_mutex);
    if(m_size == 0){
        pthread_mutex_unlock(m_mutex);
        return false;
    }
    value = m_array[m_back];
    pthread_mutex_unlock(m_mutex);
    return true;
}

template<typename T>
int block_queue<T>::size() const
{
    int tmp = 0;
    pthread_mutex_lock(m_mutex);
    tmp = m_size;
    pthread_mutex_unlock(m_mutex);
    return tmp;
}

template<typename T>
int block_queue<T>::maxSize() const
{
    int tmp = 0;
    pthread_mutex_lock(m_mutex);
    tmp = m_maxSize;
    pthread_mutex_unlock(m_mutex);
    return tmp;
}

//往队列添加元素需要唤醒所有使用队列的线程
// 元素push进入队列，相当于生产者生产了一个元素
// 若当前没有线程等待条件变量，则唤醒无意义
template<typename T>
bool block_queue<T>::push(const T& item)
{
    pthread_mutex_lock(m_mutex);
    if(m_size >= m_maxSize)
    {
        pthread_cond_broadcast(m_cond);
        pthread_mutex_unlock(m_mutex);
        return false;
    }

    m_back = (m_back + 1) % m_maxSize;
    m_array[m_back] = item;
    m_size++;
    pthread_cond_broadcast(m_cond);
    pthread_mutex_unlock(m_mutex);
    return true;
}

// pop时，如果当前队列没有元素则会等待条件变量
template<typename T>
bool block_queue<T>::pop(T& item)
{
    pthread_mutex_lock(m_mutex);
    while (m_size <= 0)
    {
        if( 0 != pthread_cond_wait(m_cond, m_mutex) ) {
            pthread_mutex_unlock(m_mutex);
            return false;
        }
    }
    // 一开始m_front 和 m_back都是 -1
    m_front = (m_front + 1) % m_maxSize;
    item = m_array[m_front];
    m_size--;
    pthread_mutex_unlock(m_mutex);
    return true;
}

//增加超时处理
template<typename T>
bool block_queue<T>::pop(T& item, int timeout)
{
    struct timespec t = {0, 0};
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    pthread_mutex_lock(m_mutex);
    if (m_size <= 0)
    {
        t.tv_sec = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;
        if (0 != pthread_cond_timedwait(m_cond, m_mutex, &t))
        {
            pthread_mutex_unlock(m_mutex);
            return false;
        }
    }

    if (m_size <= 0)
    {
        pthread_mutex_unlock(m_mutex);
        return false;
    }

    m_front = (m_front + 1) % m_maxSize;
    item = m_array[m_front];
    m_size--;
    pthread_mutex_unlock(m_mutex);
    return true;
}
