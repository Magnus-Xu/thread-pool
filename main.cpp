#include "threadpool.hpp"

#include <cstdint>
#include <iostream>
#include <cmath>

using int16 = int16_t;

constexpr int thread_num = 4;

int16 negative(int16 a)
{
	printf("hello\n");
	return -std::abs(a);
}

int16 add(int16 a, int16 b)
{
	return a + b;
}

int main()
{
	ThreadPool::thread_pool pool(thread_num);
	auto neg_future = pool.submit(&negative, 4);
	auto add_future = pool.submit(&add, 5, 7);
	auto add_future1 = pool.submit(&add, 5, 8);
	std::cout << neg_future.get() << "\n" << add_future.get() << "\n" << add_future1.get();
	return 0;
}
