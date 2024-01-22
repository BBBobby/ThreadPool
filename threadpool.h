#ifndef THREAD_POOL_
#define THREAD_POOL_

#include<unordered_map>
#include<queue>
#include<mutex>
#include<memory>
#include<atomic>
#include<functional>
#include<thread>
#include<future>
#include<iostream>
#include<condition_variable>

class Result;	// 预声明
class Any;

// 线程池模式，MODE_FIXED：固定大小，MODE_CACHE大小可变
enum class MODE {
	MODE_FIXED,
	MODE_CACHE
};


// 线程类
class Thread {
	using function = std::function<void(int)>;
public:
	Thread(function func);
	~Thread();
	void start();
	int getId();
private:
	function func_;
	static int generateId_;
	int threadId_;

};


//线程池
class ThreadPool {

public:
	ThreadPool(int initPoolSize = std::thread::hardware_concurrency());
	~ThreadPool();

	// 设置线程池工作模式
	void setMode(MODE mode);
	// 设置任务数量上限
	void setMaxTaskNum(int maxTaskNum);
	// 设置线程池最大容量
	void setMaxThreadNum(int maxThreadNum);
	// 设置上传任务的超时时间
	void setSubmitTimeout(int submitTimeout);
	// 开启线程池
	void start();	
	// 用户提交任务
	template<typename fun, typename... argsType>
	auto submitTask(fun&& func, argsType&&... args)->std::future<decltype(func(args...))> {
		using rType = decltype(func(args...));
		// 获取锁
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		// 如果任务队列还有空间则插入，否则至多等待submitTimeout_秒后退出
		if (!isFull_.wait_for(lock, std::chrono::seconds(submitTimeout_), [&]()->bool {
				return taskQue_.size() < maxTaskNum_;
			})) {	// 当返回0时，表示等待超时，返回1表示满足条件taskQue_.size() < maxTaskNum_
			std::cerr << "提交任务超时，任务队列已满" << std::endl;
			return std::future<rType>();
		}
		
		auto task = std::make_shared<std::packaged_task<rType()>>
			(std::bind(std::forward<fun>(func), std::forward<argsType>(args)...));
		std::future<rType> result = task->get_future();
		// 插入任务队列
		taskQue_.emplace([task]() {(*task)(); });
		++curTaskNum_;
		// 通知线程池来活了
		notEmpty_.notify_all();
		// cache模式，若当前任务数量大于空闲线程数量，且线程数量小于阈值，则创建新线程
		if (mode_ == MODE::MODE_CACHE && curTaskNum_ > curThreadIdleNum_ && curThreadNum_ < maxThreadNum_) {
			std::unique_ptr<Thread> t(new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)));
			int Id = t->getId();
			// 防止ID重复
			while (threads_.count(Id)) {
				t = std::unique_ptr<Thread>(new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)));
				Id = t->getId();
			}
			threads_.emplace(Id, std::move(t));
			threads_[Id]->start();
			++curThreadNum_;		// 开启线程后，线程数量+1
			++curThreadIdleNum_;	// 开启线程后，空闲数量+1
			// std::cout << "creat new thread" << std::endl;
		}
		return result;
	}

	// 禁止拷贝构造和赋值
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	void threadFunc(int threadId);
private:
	// 线程相关
	int initPoolSize_;		// 线程池初始大小
	int maxThreadNum_;		// 线程池最大容量
	std::atomic_int curThreadNum_;		// 记录当前有多少个线程
	std::atomic_int curThreadIdleNum_;	// 记录当前有多少个空闲线程
	std::unordered_map<int, std::unique_ptr<Thread>> threads_; // 线程池{threadId, threadPtr}
	// 任务相关
	int maxTaskNum_;				// 任务数量上限
	int submitTimeout_;				// 上传任务的超时时间（单位：s）
	std::atomic_int curTaskNum_;	// 任务队列中任务的数量
	using Task = std::function<void()>;
	std::queue<Task> taskQue_;		// 任务队列
	// 线程通信相关
	std::mutex taskQueMtx_;				// 任务队列的互斥锁
	std::condition_variable isFull_;	// 表示任务队列不满，可以往里添加任务
	std::condition_variable notEmpty_;	// 表示任务队列不空，需要线程处理
	std::condition_variable exitCond_;	// 通知线程池析构
	// 线程状态相关
	MODE mode_;					// 线程池模式
	std::atomic_bool isStart_;	// 标志线程池有无开启

};

#endif
