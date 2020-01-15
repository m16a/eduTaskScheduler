#include <vector>
#include <unordered_set>
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

using TID = int;

struct ITask
{
	virtual	void Call() = 0;
	virtual	TID GetTID() = 0;
	virtual	void SetTID(TID tid) = 0;
	virtual	bool IsDependOn(TID tid) = 0;
	virtual	bool HasDependentOn() = 0;
	virtual	void RemoveDependentOn(TID tid) = 0;
	virtual	void AddDependentOn(TID tid) = 0;

	virtual ~ITask() {}
};

template <typename F>
struct CTask : public ITask
{
    CTask (std::function<F>&& f) : pack(f), tid(0) {}
    std::packaged_task<F>	pack;

    virtual void Call() override
    {
        pack();
    }

	virtual	TID GetTID() override { return tid; }
	virtual void SetTID(TID id) override {
		tid = id;
	}

	virtual	bool IsDependOn(TID tid) override {
		if (dependsOn.empty())
			return false;

		if (dependsOn.find(tid) != dependsOn.end())
			return true;

		return false;
	}
	
	virtual	void RemoveDependentOn(TID tid) override {
		auto it = dependsOn.find(tid);
		dependsOn.erase(it);
	}
	
	virtual	void AddDependentOn(TID tid) override {
		dependsOn.insert(tid);
	}

	virtual	bool HasDependentOn() override {
		return !dependsOn.empty();
	}

	TID tid;
	std::unordered_set<TID> dependsOn;
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
	TaskScheduler(unsigned int n = std::thread::hardware_concurrency()) : m_ThreadsCount(n), m_last_tid(0)
	{
		stop = false;
		start = false;
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

	TID AddTask(std::shared_ptr<ITask>& t)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		
		TID tid = 0;
		if (m_free_tids.empty()) {
			tid = ++m_last_tid;
		}
		else {
			tid = m_free_tids.back();
			m_free_tids.pop_back();
		}
		t->SetTID(tid);

		m_tasks.push_back(t);

		newTask.notify_one();

		return tid;
	}

	TID AddTask(std::shared_ptr<ITask>& t, std::vector<TID> deps)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		
		TID tid = 0;
		if (m_free_tids.empty()) {
			tid = ++m_last_tid;
		}
		else {
			tid = m_free_tids.back();
			m_free_tids.pop_back();
		}
		t->SetTID(tid);

		for (auto tid : deps) {
			t->AddDependentOn(tid);
		}

		m_tasks.push_back(t);

		newTask.notify_one();

		return tid;
	}

	std::shared_ptr<ITask> PopTask()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		std::shared_ptr<ITask> task_to_call;
		for (int i = 0, size = m_tasks.size(); i < size; i++) {
			if (!m_tasks[i]->HasDependentOn()) {
				task_to_call = m_tasks[i];
				std::swap(m_tasks[i], m_tasks[size-1]);
				m_tasks.pop_back();
				break;
			}
		}
		return task_to_call;
	}

	size_t TasksCount()
	{
		return m_tasks.size(); //not thread safe;
	}

	void StartExecuting() {
		start = true;
	}
	
	private:
	std::vector<std::shared_ptr<ITask>> m_tasks;
	std::vector<std::thread> m_threads;

	const unsigned int m_ThreadsCount;
	void RunAllThreads()
	{
		for (int i=0; i < m_ThreadsCount; i++)
		{
			m_threads.emplace_back(std::thread(&TaskScheduler::RunThread, this));
		}	
	}

	void RunThread()
	{
		//std::cout << "run" << std::endl;
		
		while (!start) {}

		while (!stop)
		{
			std::unique_lock<std::mutex> lock(m_mutex2);
			newTask.wait(lock, [this]{ return !m_tasks.empty();});
			std::shared_ptr<ITask> pT = PopTask();
			lock.unlock();

			if (pT) {
				const TID tid = pT->GetTID();

				if (tid) {
					pT->Call();

					for (auto& t : m_tasks) {
						if (t->IsDependOn(tid)) {
							t->RemoveDependentOn(tid);
						}
					}

					newTask.notify_one();
				}
			}


		}	
	}

	std::condition_variable newTask;
	std::atomic<bool> stop;
	std::atomic<bool> start;
	std::mutex m_mutex;
	std::mutex m_mutex2;

	std::vector<TID> m_free_tids;
	TID m_last_tid;
};

int main()
{
#if INTEGRATE_EASY_PROFILER
	profiler::startListen();
#endif

	TaskScheduler ts;

	/*
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
*/

	TID tid1;
	{
		auto lambdaFunc = []() { std::cout << "Task 1" << std::endl; };
		auto pT = new CTask<void()>(lambdaFunc);
		auto shared = std::shared_ptr<ITask>(pT);
		tid1 = ts.AddTask(shared);
	}
	
	TID tid2;
	{
		auto lambdaFunc = []() { std::cout << "Task 2" << std::endl; };
		auto pT = new CTask<void()>(lambdaFunc);
		auto shared = std::shared_ptr<ITask>(pT);
		tid2 = ts.AddTask(shared);
	}
	
	TID tid3;
	{
		auto lambdaFunc = []() { std::cout << "Task 3" << std::endl; };
		auto pT = new CTask<void()>(lambdaFunc);
		auto shared = std::shared_ptr<ITask>(pT);
		tid3 = ts.AddTask(shared, {tid1, tid2});
	}

	TID tid4;
	{
		auto lambdaFunc = []() { std::cout << "Task 4" << std::endl; };
		auto pT = new CTask<void()>(lambdaFunc);
		auto shared = std::shared_ptr<ITask>(pT);
		tid4 = ts.AddTask(shared);
	}

	TID tid5;
	{
		auto lambdaFunc = []() { std::cout << "Task 5" << std::endl; };
		auto pT = new CTask<void()>(lambdaFunc);
		auto shared = std::shared_ptr<ITask>(pT);
		tid5 = ts.AddTask(shared, {tid3, tid4 });
	}

	ts.StartExecuting();

	while (true)
	{
		std::cout << "tsks cnt: " << ts.TasksCount() << std::endl;

#ifdef WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}

	return 0;
}
