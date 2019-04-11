#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <queue>
#include <chrono>
#include <memory>
#include <atomic>
#include <functional>

namespace threadsafe_queue {


	template<typename T>
	class threadsafeQueue
	{
	private:

		mutable std::mutex mut;
		std::queue<std::shared_ptr<T>> dataQueue;
		std::condition_variable cond;
	public:

		threadsafeQueue()
		{}

		void wait_and_pop(T& value)
		{
			std::unique_lock<std::mutex> lk(mut);
			cond.wait(lk, [this] {return !dataQueue.empty(); });

			value = std::move(*dataQueue.front());
			dataQueue.pop();
		}

		bool try_pop(T& value)
		{
			std::lock_guard<std::mutex> lk(mut);
			if (dataQueue.empty())
				return false;

			value = std::move(*dataQueue.front());
			dataQueue.pop();

			return true;
		}

		std::shared_ptr<T> wait_and_pop()
		{
			std::unique_lock<std::mutex> lk(mut);
			cond.wait(lk, [this] {return !dataQueue.empty(); });

			std::shared_ptr<T> res = dataQueue.front();
			dataQueue.pop();
			return res;
		}

		std::shared_ptr<T> try_pop()
		{
			std::lock_guard<std::mutex> lk(mut);
			if (dataQueue.empty())
				return std::shared_ptr<T>();

			std::shared_ptr<T> res = dataQueue.front();
			dataQueue.pop();
			return res;
		}

		void push(T new_value)
		{
			std::shared_ptr<T> data(
				std::make_shared<T>(std::move(new_value)));

			std::lock_guard<std::mutex> lk(mut);
			dataQueue.push(data);
			cond.notify_one();
		}

		bool empty() const
		{
			std::lock_guard<std::mutex> lk(mut);
			return dataQueue.empty();
		}
	};
}


namespace thread_pool {

	// -------------------------------Thread joiner----------------------------------------------
	class joinThreads
	{
		std::vector<std::thread>& threads;

	public:

		explicit joinThreads(std::vector<std::thread>& threads_)
			: threads(threads_)
		{}

		~joinThreads()
		{
			for (unsigned int i = 0; i < threads.size(); ++i)
				if (threads[i].joinable())
					threads[i].join();
		}
	};

	// ------------------------------------Function wrapper-----------------------------------------

	class functionWrapper
	{
	private:

		struct impl_base
		{
			virtual void call() = 0;
			virtual ~impl_base() {};
		};


		template<typename FunctionType>
		struct impl_type : impl_base
		{
			FunctionType f;

			impl_type(FunctionType&& fun)
				: f(std::move(fun)) {}

			void call()
			{
				f();
			}
		};

		std::unique_ptr<impl_base> impl;

	public:

		template<typename FunctionType>
		functionWrapper(FunctionType&& fun)
			: impl(new impl_type<FunctionType>(std::move(fun))) {}

		void operator()()
		{
			impl->call();
		}

		functionWrapper() = default;

		functionWrapper(functionWrapper&& other)
			: impl(std::move(other.impl)) {}

		functionWrapper& operator=(functionWrapper&& other)
		{
			impl = std::move(other.impl);
			return *this;
		}

		functionWrapper(const functionWrapper&) = delete;
		functionWrapper(functionWrapper&) = delete;
		functionWrapper& operator=(const functionWrapper&) = delete;
	};

	// ---------------------------------Thread pool--------------------------------------------

	class threadPool
	{
		std::atomic_bool done;
		threadsafe_queue::threadsafeQueue<functionWrapper> workQueue;
		std::vector<std::thread> threads;
		joinThreads joiner;

		void runPendingTask()
		{
			functionWrapper task;
			if (workQueue.try_pop(task))
				task();
			else
				std::this_thread::yield();
		}

		void workerThread()
		{
			while (!done)
			{
				runPendingTask();

				std::this_thread::sleep_for(std::chrono::milliseconds(256));
			}
		}

	public:

		threadPool()
			: done(false), joiner(threads)
		{
			unsigned int threadAmount = std::thread::hardware_concurrency();

			try
			{
				for (unsigned int i = 0; i < threadAmount; ++i)
					threads.push_back(
						std::thread(&threadPool::workerThread, this));
			}
			catch (...)
			{
				done = true;
				throw;
			}
		}

		~threadPool()
		{
			done = true;
		}

		template<typename FunctionType, typename resultType = std::result_of_t<FunctionType()>>
		std::future<resultType> submit(FunctionType f)
		{
			std::packaged_task<resultType()> task(std::move(f));
			std::future<resultType> futureRes(task.get_future());
			workQueue.push(std::move(task));

			return futureRes;
		}

	};
}

