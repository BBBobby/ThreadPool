#include"threadpool.h"
/////////////////////////////////////////////////////ThreadPool
const int MAX_TASK_NUM = 1024;		// 最大任务数量
const int MAX_Thread_NUM = 10;		// 最大线程数量
const int MAX_IDLE_TIME = 10;		// cache模式下，额外线程最长空闲时间
const int SIBMIT_TIMEOUT = 1;		// 上传任务的超时时间
ThreadPool::ThreadPool(int initPoolSize) :
	submitTimeout_(SIBMIT_TIMEOUT),
	initPoolSize_(initPoolSize),
	curThreadNum_(0),
	curThreadIdleNum_(0),
	maxTaskNum_(MAX_TASK_NUM),
	maxThreadNum_(MAX_Thread_NUM),
	curTaskNum_(0),
	isStart_(false),
	mode_(MODE::MODE_FIXED){}

// 析构函数
ThreadPool::~ThreadPool() {
	isStart_ = false;

	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// 要析构时，线程有三种情况（1）还在执行任务，（2）因为任务队列为空而wait，（3）在抢锁
	// 下面一句不能放在	std::unique_lock<std::mutex> lock(taskQueMtx_)之上，因为这样可能导致在（3）情况下发生死锁
	notEmpty_.notify_all();	// 因为任务队列为空而wait，需要唤醒。
	// 直到线程池为空退出
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

// 设置线程池工作模式
void ThreadPool::setMode(MODE mode) {
	if (isStart_) {
		return;
	}
	mode_ = mode;
}

// 设置任务数量上限
void ThreadPool::setMaxTaskNum(int maxTaskNum) {
	if (!isStart_ || maxTaskNum < 0) {
		std::cout << "操作无效：线程池已析构 / maxTaskNum < 0" << std::endl;
		return;
	}
	maxTaskNum_ = maxTaskNum;
}

// 设置最大线程数量
void ThreadPool::setMaxThreadNum(int maxThreadNum) {
	if (!isStart_ || mode_ != MODE::MODE_CACHE || maxThreadNum < initPoolSize_) {
		std::cout << "操作无效：线程池已析构 / 非cached模式 / maxThreadNum < 0" << std::endl;
		return;
	}
	maxThreadNum_ = maxThreadNum;
}

// 设置上传任务的超时时间
void ThreadPool::setSubmitTimeout(int submitTimeout) {
	if (!isStart_ || submitTimeout <= 0) {
		std::cout << "操作无效：线程池已析构 / submitTimeout <= 0" << std::endl;
		return;
	}
	submitTimeout_ = submitTimeout;
}

// 开启线程池
void ThreadPool::start() {
	isStart_ = true;
	for (int i = 0; i < initPoolSize_; ++i) {
		std::unique_ptr<Thread> t(new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)));
		threads_.emplace(t->getId(), std::move(t));
	}
	// 这里可以合到上面去。
	for (auto& item: threads_) {
		item.second->start();
		++curThreadNum_;		// 开启线程后，线程数量+1
		++curThreadIdleNum_;	// 开启线程后，空闲数量+1
	}
}

void ThreadPool::threadFunc(int threadId) {
	for (;;) {

		Task task;
		{
			// 获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			while (taskQue_.size() == 0) {
				// 线程池析构了就ruturn
				if (isStart_ == false) {
					//std::cout << threads_[threadId]->getId() << "out" << std::endl;
					threads_.erase(threadId);
					exitCond_.notify_all();
					return;
				}
				if (mode_ == MODE::MODE_CACHE) {
					// 多余的线程空闲时间过长就释放
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(MAX_IDLE_TIME))) {
						if (curThreadNum_ > initPoolSize_) {
							std::cout << threads_[threadId]->getId() << "out" << std::endl;
							threads_.erase(threadId);
							--curThreadNum_;
							--curThreadIdleNum_;
							return;
						}
					}
				}
				else {
					// 若任务队列为空，则等待
					notEmpty_.wait(lock);
				}
			}
			--curThreadIdleNum_;	// 处理任务，则空闲线程-1
			task = taskQue_.front();
			taskQue_.pop();
			--curTaskNum_;
			// 若任务队列不为空，告诉其他（好像可以不要）
			if (taskQue_.size() > 0) {
				notEmpty_.notify_all();
			}
			// 已经从任务队列中取出任务，那么队列必不满
			isFull_.notify_all();
		}
		if (task != nullptr) {
			task();
		}
		++curThreadIdleNum_;	// 处理完任务，则空闲线程+1
	}

}

///////////////////////////////////////////////////Thread
int Thread::generateId_ = 0;

Thread::Thread(function func): func_(func), threadId_(++generateId_) {}

Thread::~Thread() {}

int Thread::getId() {
	return threadId_;
}

void Thread::start() {
	std::thread t(func_, threadId_);
	t.detach();
}





