# ThreadPool 基于C++11实现线程池
## 基本功能
1、基于可变参模板，引用折叠，后置返回类型，实现线程池submitTask接口，支持用户传入任意类型的任务函数和任意个数/类型参数的传递。

2、使用future和packaged_task实现异步获取结果的机制。 

3、使用unordered_map管理线程对象（key为线程Id），使用queue管理任务

4、基于条件变量condition_variable和互斥锁mutex实现任务提交线程和任务执行线程间的通信机制。

5、支持fixed和cached模式的线程池定制（cached模式即线程池大小可根据需求动态变化）

## 核心代码思想
### Thread类
1、构造函数接收一个线程函数，用std::function<void(int)>包装,里面的int是接收线程Id；线程id也在该构造函数内设置。

2、void Thread::start()：开启一个线程，传入线程函数，设置为分离线程。

### ThreadPool类
1、使用unordered_map和unique_ptr来管理线程对象（用哈希表是为了记录线程id，key为线程Id；用unique_ptr是因为这个场景下无需多个指针共享操作一个对象，只在开启线程池和从map中销毁时才需要使用，用unique_ptr更轻量）<br>
	std::unordered_map<int, std::unique_ptr<Th Thread::start()read>> threads_;
 
2、使用queue来管理任务，使用std::function<void()>包装用户传入的任务函数<br>
	using Task = std::function<void()>;<br>
	std::queue<Task> taskQue_;		
 
3、void ThreadPool::threadFunc(int threadId)：线程函数<br>
（1）所有线程执行的是同一个线程函数threadFunc。<br>
（2）线程函数中是一个for(;;)循环，循环内部获取锁后查看任务队列里是否有任务需要处理，有则取出进行处理，无则notEmpty_.wait(lock);<br>
（3）在缓存模式下，如果任务队列为空就会找出空间时间过长的线程，释放。

4、void ThreadPool::start()：开启线程池<br>
	该函数内部创建initPoolSize_个Thread对象，通过绑定器将线程函数和this指针绑定，占位符为线程id<br>
new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1))<br>
（1）创建的Thread对象传入unordered_map中，注意因为unique_ptr只提供了移动赋值函数，因此需要使用std::move()；此外，因为key为线程id，因此需要调用Thread::getId()。<br>
（2）调用Thread::start()开启线程。

5、提交任务函数<br>
template<typename fun, typename... argsType><br>
auto submitTask(fun&& func, argsType&&... args)->std::future<decltype(func(args...))><br>
（1）用户通过该函数提交任务函数和相应的参数。<br>
（2）因为不能事先知道用户提交的函数类型和参数类型，因此使用引用折叠和可变参模板。<br>
（3）因为不能事先知道用户提交的任务函数返回值，因此使用后置返回类型的写法。<br>
（4）返回类型即为using rType = decltype(func(args...));
（5）若任务队列已满，则至多等待submitTimeout_，超时则返回一个空std::future<rType>()<br>
（6）因为要支持异步获取返回值，使用std::packaged_task<rType>()来包装执行函数，首先用std::bind()将函数与参数绑定（需要保持函数和参数的左值或右值类型，因此这里要用类型的完美转发std::forward）。<br>
（7）用shared_ptr来管理这个packaged_task。这是因为要延长这个task的生命周期，所以得通过new,然后用智能指针来管理。<br>
（8）将packaged_task包装好的task通过get_future()与std::future<rType>绑定。后续将这个返回给用户。<br>
（9）将task插入任务队列，注意，任务队列里的类型是std::function<void()>,而此时task的类型是rType(),因此需要用lambda再包装一下	：taskQue_.emplace(\[task\]() {(*task)(); });
（10）通知线程池来活了：notEmpty_.notify_all();<br>
（11）cache模式下，若任务数量大于线程数量，且线程数量小于最大阈值，创建新的线程。<br>

