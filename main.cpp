#include "test_listener.hpp"

using namespace test_listener;

int main()
{
	listener listener_instance(32, 5555, "127.0.0.1");
	
	listener_instance.run();
	
	return 0;
}
