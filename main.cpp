#include <vector>
#include <thread>
#include <queue>
#include <iostream>
#include <condition_variable>
#include <atomic>
#include <future>
#include <memory>
#include <cmath>
#ifdef WIN32
#include <windows.h>
#else
//#include <unistd.h>
#endif

//defined in CMakeList.txt
#ifdef INTEGRATE_EASY_PROFILER
#define BUILD_WITH_EASY_PROFILER
#include <easy/profiler.h>
#else
#define EASY_FUNCTION(a)
#endif

struct ITask
{
	virtual	void Call() = 0;
	virtual ~ITask() {}
};

template <typename F>
struct CTask : public ITask
{
    CTask (std::function<F>&& f) : pack(f) {}
    std::packaged_task<F>	pack;

    virtual void Call() override
    {
        pack();
    }

};

double Payload()
{
	EASY_FUNCTION();
	double sum = 0.0;
	int sign = 1;
	for (int i = 0; i < 1000000; ++i) {           
			sum += sign/(2.0*i+1.0);
			sign *= -1;
	}
	return 4.0*sum;
}

float Payload2()
{
	EASY_FUNCTION(profiler::colors::Magenta);
	float sum = 0.0f;
	for (int i = 0; i < 100000; i++)
	{
		sum += sin(i) * cos(i);
	}
	return sum;
}

class TaskScheduler
{
	public:
	TaskScheduler(unsigned int n = std::thread::hardware_concurrency()) : m_ThreadsCount(n)
	{
		stop = false;
		RunAllThreads();
	}

	~TaskScheduler()
	{
		stop = true;

		for (int i=0; i<m_ThreadsCount; i++)
		{
			m_threads[i].join();
		}	
	}

	void AddTask(std::shared_ptr<ITask>& t)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_tasks.push(t);

		newTask.notify_one();
	}

	std::shared_ptr<ITask> PopTask()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		
		std::shared_ptr<ITask> t = m_tasks.front();		
		m_tasks.pop();
		return t;
	}

	size_t TasksCount()
	{
		return m_tasks.size(); //not thread safe;
	}
	
	private:
	std::queue<std::shared_ptr<ITask>> m_tasks;
	std::vector<std::thread> m_threads;

	const unsigned int m_ThreadsCount;
	void RunAllThreads()
	{
		for (int i=0; i<m_ThreadsCount; i++)
		{
			m_threads.emplace_back(std::thread(&TaskScheduler::RunThread, this));
		}	
	}

	void RunThread()
	{
		std::cout << "run" << std::endl;
		
		while (!stop)
		{
			std::unique_lock<std::mutex> lock(m_mutex2);
			newTask.wait(lock, [this]{ return !m_tasks.empty();});
			std::shared_ptr<ITask> pT = PopTask();
			lock.unlock();

			pT->Call();
		}	
	}

	std::condition_variable newTask;
	std::atomic<bool> stop;
	std::mutex m_mutex;
	std::mutex m_mutex2;
};

int main()
{
#if INTEGRATE_EASY_PROFILER
	profiler::startListen();
#endif

	TaskScheduler ts;

	for (int i=0; i<20000; ++i)
	{
	    auto lambdaFunc = [](){Payload();};
		auto pT = new CTask<void()>(lambdaFunc);
		auto shared = std::shared_ptr<ITask>(pT);
		ts.AddTask(shared);
	}

	for (int i=0; i<10000; ++i)
	{
		auto pT = new CTask<float(void)>(Payload2);
		auto shared = std::shared_ptr<ITask>(pT);
		ts.AddTask(shared);
	}

	{
		auto pT = new CTask<float(void)>(std::function<float(void)>(Payload));
		auto future = pT->pack.get_future();
		auto shared = std::shared_ptr<ITask>(pT);
		ts.AddTask(shared);

		//future.get(); //wait all task, comment to skip
	}

	while (true)
	{
		std::cout << "Tasks: " << ts.TasksCount() << std::endl;

#ifdef WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}

	return 0;
}
