// geeksforgeeks inspired
// C++ Program to demonstrate thread pooling

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
using namespace std;


std::atomic<int> finished{0};
int total_tasks = 0;
// Class that represents a simple thread pool
class ThreadPool {
public:
    // // Constructor to creates a thread pool with given
    // number of threads
    ThreadPool(size_t num_threads
               = thread::hardware_concurrency())
    {

        // Creating worker threads
        for (size_t i = 0; i < num_threads; ++i) {
            threads_.emplace_back([this] {
                while (true) {
                    function<void()> task;
                    // The reason for putting the below code
                    // here is to unlock the queue before
                    // executing the task so that other
                    // threads can perform enqueue tasks
                    {
                        // Locking the queue so that data
                        // can be shared safely
                        unique_lock<mutex> lock(
                            queue_mutex_);

                        // Waiting until there is a task to
                        // execute or the pool is stopped
                        cv_.wait(lock, [this] {
                            return !tasks_.empty() || stop_;
                        });

                        // exit the thread in case the pool
                        // is stopped and there are no tasks
                        if (stop_ && tasks_.empty()) {
                            return;
                        }

                        // Get the next task from the queue
                        task = move(tasks_.front());
                        tasks_.pop();
                    }

                    task();
                }
            });
        }
    }

    // Destructor to stop the thread pool
    ~ThreadPool()
    {
        {
            // Lock the queue to update the stop flag safely
            unique_lock<mutex> lock(queue_mutex_);
            stop_ = true;
        }

        // Notify all threads
        cv_.notify_all();

        // Joining all worker threads to ensure they have
        // completed their tasks
        for (auto& thread : threads_) {
            thread.join();
        }
    }

    // Enqueue task for execution by the thread pool
    void enqueue(function<void()> task)
    {
        {
            unique_lock<std::mutex> lock(queue_mutex_);
            tasks_.emplace(move(task));
        }
        cv_.notify_one();
    }

private:
    // Vector to store worker threads
    vector<thread> threads_;

    // Queue of tasks
    queue<function<void()> > tasks_;

    // Mutex to synchronize access to shared data
    mutex queue_mutex_;

    // Condition variable to signal changes in the state of
    // the tasks queue
    condition_variable cv_;

    // Flag to indicate whether the thread pool should stop
    // or not
    bool stop_ = false;
};

void proccess_tile(int tile_location){
    // make thread do "work" aka proccess smth
    // call function later
    this_thread::sleep_for(chrono::milliseconds(30));
    finished.fetch_add(1, std::memory_order_relaxed);
}



void print_progress_bar(int completed, int total, int bar_width = 50) {
    double progress = double(completed) / total;
    int pos = static_cast<int>(bar_width * progress);

    cout << "\r[";  // carriage return to overwrite the line
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) cout << "=";
        else if (i == pos) cout << ">";
        else cout << " ";
    }
    cout << "] " << int(progress * 100.0) << "% (" << completed << "/" << total << ")   " << flush;
}

int main()
{
    // Create a thread pool with 4 threads
    ThreadPool pool(4);
    // number of tasks
    int num_tasks = 1000;
    total_tasks = num_tasks;
    int complete = 0;

    // Enqueue tasks for execution
    for (int i = 0; i < num_tasks; ++i) {
        pool.enqueue([i] {
            // cout << "Task " << i << " is running on thread " << this_thread::get_id() << endl;
            // do smth
            proccess_tile(i);
        });
    }
    // get current time
    auto start = chrono::steady_clock::now();
    
    while (finished.load() < num_tasks){
        this_thread::sleep_for(chrono::milliseconds(80));
        complete = finished.load();
        // Progress is as  a percentage of compelte over total taks
        // cout << complete << total_tasks << endl;
        double progress = 100.0 * complete / total_tasks;
        print_progress_bar(complete, total_tasks);

        // cout << "Proccessed: " << complete << "/" << num_tasks << " (" << progress << "%)" << endl;

    }

    auto end = chrono::steady_clock::now();
    double time_taken = chrono::duration<double>(end - start).count();

    cout << 4 << "threads finished in " << time_taken << "s" << endl;

    return 0;
}