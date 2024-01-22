#include"threadpool.h"
/////////////////////////////////////////////////////ThreadPool
const int MAX_TASK_NUM = 1024;		// �����������
const int MAX_Thread_NUM = 10;		// ����߳�����
const int MAX_IDLE_TIME = 10;		// cacheģʽ�£������߳������ʱ��
const int SIBMIT_TIMEOUT = 1;		// �ϴ�����ĳ�ʱʱ��
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

// ��������
ThreadPool::~ThreadPool() {
	isStart_ = false;

	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// Ҫ����ʱ���߳������������1������ִ�����񣬣�2����Ϊ�������Ϊ�ն�wait����3��������
	// ����һ�䲻�ܷ���	std::unique_lock<std::mutex> lock(taskQueMtx_)֮�ϣ���Ϊ�������ܵ����ڣ�3������·�������
	notEmpty_.notify_all();	// ��Ϊ�������Ϊ�ն�wait����Ҫ���ѡ�
	// ֱ���̳߳�Ϊ���˳�
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

// �����̳߳ع���ģʽ
void ThreadPool::setMode(MODE mode) {
	if (isStart_) {
		return;
	}
	mode_ = mode;
}

// ����������������
void ThreadPool::setMaxTaskNum(int maxTaskNum) {
	if (!isStart_ || maxTaskNum < 0) {
		std::cout << "������Ч���̳߳������� / maxTaskNum < 0" << std::endl;
		return;
	}
	maxTaskNum_ = maxTaskNum;
}

// ��������߳�����
void ThreadPool::setMaxThreadNum(int maxThreadNum) {
	if (!isStart_ || mode_ != MODE::MODE_CACHE || maxThreadNum < initPoolSize_) {
		std::cout << "������Ч���̳߳������� / ��cachedģʽ / maxThreadNum < 0" << std::endl;
		return;
	}
	maxThreadNum_ = maxThreadNum;
}

// �����ϴ�����ĳ�ʱʱ��
void ThreadPool::setSubmitTimeout(int submitTimeout) {
	if (!isStart_ || submitTimeout <= 0) {
		std::cout << "������Ч���̳߳������� / submitTimeout <= 0" << std::endl;
		return;
	}
	submitTimeout_ = submitTimeout;
}

// �����̳߳�
void ThreadPool::start() {
	isStart_ = true;
	for (int i = 0; i < initPoolSize_; ++i) {
		std::unique_ptr<Thread> t(new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)));
		threads_.emplace(t->getId(), std::move(t));
	}
	// ������Ժϵ�����ȥ��
	for (auto& item: threads_) {
		item.second->start();
		++curThreadNum_;		// �����̺߳��߳�����+1
		++curThreadIdleNum_;	// �����̺߳󣬿�������+1
	}
}

void ThreadPool::threadFunc(int threadId) {
	for (;;) {

		Task task;
		{
			// ��ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			while (taskQue_.size() == 0) {
				// �̳߳������˾�ruturn
				if (isStart_ == false) {
					//std::cout << threads_[threadId]->getId() << "out" << std::endl;
					threads_.erase(threadId);
					exitCond_.notify_all();
					return;
				}
				if (mode_ == MODE::MODE_CACHE) {
					// ������߳̿���ʱ��������ͷ�
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
					// ���������Ϊ�գ���ȴ�
					notEmpty_.wait(lock);
				}
			}
			--curThreadIdleNum_;	// ��������������߳�-1
			task = taskQue_.front();
			taskQue_.pop();
			--curTaskNum_;
			// ��������в�Ϊ�գ�����������������Բ�Ҫ��
			if (taskQue_.size() > 0) {
				notEmpty_.notify_all();
			}
			// �Ѿ������������ȡ��������ô���бز���
			isFull_.notify_all();
		}
		if (task != nullptr) {
			task();
		}
		++curThreadIdleNum_;	// ����������������߳�+1
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





