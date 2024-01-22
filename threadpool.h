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

class Result;	// Ԥ����
class Any;

// �̳߳�ģʽ��MODE_FIXED���̶���С��MODE_CACHE��С�ɱ�
enum class MODE {
	MODE_FIXED,
	MODE_CACHE
};


// �߳���
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


//�̳߳�
class ThreadPool {

public:
	ThreadPool(int initPoolSize = std::thread::hardware_concurrency());
	~ThreadPool();

	// �����̳߳ع���ģʽ
	void setMode(MODE mode);
	// ����������������
	void setMaxTaskNum(int maxTaskNum);
	// �����̳߳��������
	void setMaxThreadNum(int maxThreadNum);
	// �����ϴ�����ĳ�ʱʱ��
	void setSubmitTimeout(int submitTimeout);
	// �����̳߳�
	void start();	
	// �û��ύ����
	template<typename fun, typename... argsType>
	auto submitTask(fun&& func, argsType&&... args)->std::future<decltype(func(args...))> {
		using rType = decltype(func(args...));
		// ��ȡ��
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		// ���������л��пռ�����룬��������ȴ�submitTimeout_����˳�
		if (!isFull_.wait_for(lock, std::chrono::seconds(submitTimeout_), [&]()->bool {
				return taskQue_.size() < maxTaskNum_;
			})) {	// ������0ʱ����ʾ�ȴ���ʱ������1��ʾ��������taskQue_.size() < maxTaskNum_
			std::cerr << "�ύ����ʱ�������������" << std::endl;
			return std::future<rType>();
		}
		
		auto task = std::make_shared<std::packaged_task<rType()>>
			(std::bind(std::forward<fun>(func), std::forward<argsType>(args)...));
		std::future<rType> result = task->get_future();
		// �����������
		taskQue_.emplace([task]() {(*task)(); });
		++curTaskNum_;
		// ֪ͨ�̳߳�������
		notEmpty_.notify_all();
		// cacheģʽ������ǰ�����������ڿ����߳����������߳�����С����ֵ���򴴽����߳�
		if (mode_ == MODE::MODE_CACHE && curTaskNum_ > curThreadIdleNum_ && curThreadNum_ < maxThreadNum_) {
			std::unique_ptr<Thread> t(new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)));
			int Id = t->getId();
			// ��ֹID�ظ�
			while (threads_.count(Id)) {
				t = std::unique_ptr<Thread>(new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)));
				Id = t->getId();
			}
			threads_.emplace(Id, std::move(t));
			threads_[Id]->start();
			++curThreadNum_;		// �����̺߳��߳�����+1
			++curThreadIdleNum_;	// �����̺߳󣬿�������+1
			// std::cout << "creat new thread" << std::endl;
		}
		return result;
	}

	// ��ֹ��������͸�ֵ
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	void threadFunc(int threadId);
private:
	// �߳����
	int initPoolSize_;		// �̳߳س�ʼ��С
	int maxThreadNum_;		// �̳߳��������
	std::atomic_int curThreadNum_;		// ��¼��ǰ�ж��ٸ��߳�
	std::atomic_int curThreadIdleNum_;	// ��¼��ǰ�ж��ٸ������߳�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_; // �̳߳�{threadId, threadPtr}
	// �������
	int maxTaskNum_;				// ������������
	int submitTimeout_;				// �ϴ�����ĳ�ʱʱ�䣨��λ��s��
	std::atomic_int curTaskNum_;	// ������������������
	using Task = std::function<void()>;
	std::queue<Task> taskQue_;		// �������
	// �߳�ͨ�����
	std::mutex taskQueMtx_;				// ������еĻ�����
	std::condition_variable isFull_;	// ��ʾ������в��������������������
	std::condition_variable notEmpty_;	// ��ʾ������в��գ���Ҫ�̴߳���
	std::condition_variable exitCond_;	// ֪ͨ�̳߳�����
	// �߳�״̬���
	MODE mode_;					// �̳߳�ģʽ
	std::atomic_bool isStart_;	// ��־�̳߳����޿���

};

#endif
