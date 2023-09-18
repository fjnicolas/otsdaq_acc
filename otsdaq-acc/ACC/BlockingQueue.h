#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>

template<typename T>
class BlockingQueue : public std::queue<T>
{
public:
    BlockingQueue() : std::queue<T>()
    {
    }

    void push(T&& data)
    {
        std::unique_lock<std::mutex> lock(mut_);
        
        std::queue<T>::push(data);
        block_.notify_all();
    }

    template <class... Args> 
    void emplace(Args&&... args)
    {
        std::unique_lock<std::mutex> lock(mut_);
        
        std::queue<T>::emplace(args...);
        block_.notify_all();
    }

    const T& front()
    {
        std::unique_lock<std::mutex> lock(mut_);
            
        if(std::queue<T>::size() <= 0)
        {
            block_.wait(lock);
        }
        
        return std::queue<T>::front();
    }

    void pop()
    {
        std::unique_lock<std::mutex> lock(mut_);

        if(std::queue<T>::size() <= 0)
        {
            block_.wait(lock);
        }

        //T data = std::move(std::queue<T>::front());
        std::queue<T>::pop();
        //return std::move(data);
    }

private:
    std::mutex mut_;
    std::condition_variable block_;
};
