#include"threadpool.h"
#include<iostream>
#include<thread>


int test(int a, int b) {
	return a + b;
}



int main() {



	ThreadPool pool(1);
	pool.start();
	std::future<int> result = pool.submitTask(test, 1, 2);
	if (result.valid()) {
		std::cout << result.get() << std::endl;
	}

	std::this_thread::sleep_for(std::chrono::seconds(2));
	std::cout << "return" << std::endl;
}