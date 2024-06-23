#include <iostream>
#include <thread>

class A
{
public:
	A(int m): i(m) {
		printf("ctor %lld\n", std::this_thread::get_id());
	}
	A operator=(A& a) {
		if (this == &a) {
			return *this;
		}
		this->i = a.i;
		printf("operator %lld\n", std::this_thread::get_id());
	}
private:
	int i;
};

void f(A a) {
	printf("%lld\n", sizeof(A));
}

int main()
{
	A(5);
	std::thread a(f, 5);
	a.join();
}
