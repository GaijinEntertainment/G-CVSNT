#pragma once
#include <queue>
#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>
;
template<typename Data>
class concurrent_queue
{
private:
  std::deque<Data> the_queue;
  mutable std::mutex the_mutex;
  std::condition_variable the_condition;
  enum class Status {Normal, Finish, Cancel} status = Status::Normal;

  //All threads that might be waiting on this queue
  std::vector<std::thread> * waiting_threads = nullptr;

public:
  concurrent_queue( std::vector<std::thread> * waiting_threads_ ) : waiting_threads(waiting_threads_) {}
  ~concurrent_queue() {finishWork();}
  void finishWork()
  {
    cancel(Status::Finish);
    waitAll();
  }
  void cancel()
  {
    cancel(Status::Cancel);
    waitAll();
  }
  void waitAll()
  {
    if (!waiting_threads)
      return;
    for (auto &e:*waiting_threads)
      e.join();
    std::unique_lock<std::mutex> lock(the_mutex);
    waiting_threads->clear();
  }


  void push(Data const& data)
  {
    std::unique_lock<std::mutex> lock(the_mutex);
    if (status == Status::Cancel)//we allow to add jobs even when finishing
      return;
    the_queue.push_back(data);
    lock.unlock();
    the_condition.notify_one();
  }

  void emplace(Data && data)
  {
    std::unique_lock<std::mutex> lock(the_mutex);
    if (status == Status::Cancel)//we allow to add jobs even when finishing
      return;
    the_queue.emplace_back(std::move(data));
    lock.unlock();
    the_condition.notify_one();
  }
  void push(Data && data)
  {
    emplace(std::move(data));
  }

  bool empty() const
  {
    std::unique_lock<std::mutex> lock(the_mutex);
    if (status == Status::Cancel)
      return true;
    return the_queue.empty();
  }

  bool try_pop(Data& popped_value)
  {
    std::unique_lock<std::mutex> lock(the_mutex);
    if (status == Status::Cancel || the_queue.empty())
      return false;

    popped_value=std::move(the_queue.front());
    the_queue.pop_front();
    return true;
  }

  bool wait_and_pop(Data& popped_value)//returns false if was canceled and there is no work
  {
    std::unique_lock<std::mutex> lock(the_mutex);

    while (the_queue.empty() && status == Status::Normal)
      the_condition.wait(lock);
    if (the_queue.empty())
      return false;
    std::swap(popped_value, the_queue.front());
    the_queue.pop_front();
    return true;
  }

  void cancel(Status st)
  {
    std::unique_lock<std::mutex> lock(the_mutex);
    if ((int)status > (int)st)
      return;
    status = st;
    lock.unlock();
    the_condition.notify_all();
  }
};