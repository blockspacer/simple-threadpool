

#include "pch.h"
#include <iostream>

#include "threadpool.hpp"

using std::cin;
using std::cout;
using std::endl;


void foo()
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	cout << "second" << endl;
}


int main()
{

	thread_pool::threadPool pool;

	auto fut1 = pool.submit([]() {cout << "first" << endl; std::this_thread::sleep_for(std::chrono::seconds(2)); cout << "first again" << endl;  });
	auto fut2 = pool.submit(foo);
	
	fut1.wait(); fut2.wait();

	system("pause");
	return 0;
}

