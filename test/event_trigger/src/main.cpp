#include <wawo.h>
//#include "server_node.hpp"

#include <iostream>

//typedef int(*foo_t)(int);
//typedef std::function<int(int)> foo_tt;

void fooo(int arg) {
	WAWO_INFO("arg: %d", arg);
//	return arg;
}

typedef decltype(fooo) _fooo_t;
typedef decltype( std::bind(&fooo, std::placeholders::_1) ) fooo_t;

typedef std::function<void(int)> event_handler_int_t;
typedef std::function<void(int, int)> event_handler_int_int_t;

class user_event_trigger :
	public wawo::event_trigger
{
public:
	void invoke_int(int i) {
		event_trigger::invoke<event_handler_int_t>(1, i);
	}
	void invoke_int_int(int a, int b) {
		event_trigger::invoke<event_handler_int_int_t>(2, a, b);
	}
};

class user_event_handler:
	public wawo::ref_base
{
public:
	long call_count;
	user_event_handler() :call_count(0)
	{}

	void foo_int(int arg) {
		printf("cfoo::foo(), arg: %d", arg);
		call_count++;
	}
	void foo_int_int(int a, int b) {
		printf("cfoo::foo(), arg: %d, %d", a, b);
		call_count++;
	}
};


int main(int argc, char** argv) {

	WWRP<user_event_trigger> user_et = wawo::make_ref<user_event_trigger>();
	WWRP<user_event_handler> user_evh =  wawo::make_ref<user_event_handler>();

	event_handler_int_t lambda;
	lambda = [&user_evh](int arg) -> void {
		user_evh->foo_int(arg);
	};

	event_handler_int_int_t lambda2 = [&user_evh](int a, int b) -> void {
		user_evh->foo_int_int(a, b);
		(void)b;
	};

	user_et->bind(1, lambda);
	user_et->bind<event_handler_int_t>(1, [&user_evh](int a) {
		user_evh->foo_int(a);
	});

	user_et->invoke_int(10);
	assert(user_evh->call_count == 2);

	user_et->bind(2, lambda2);
	user_et->invoke_int_int(2, 2);
	assert(user_evh->call_count == 3);

	user_et->invoke_int_int(2, 2);
	assert(user_evh->call_count == 4);

	event_handler_int_t h_frombind = std::bind(&user_event_handler::foo_int, user_evh, std::placeholders::_1);

	user_et->bind<event_handler_int_t>(3,&user_event_handler::foo_int, user_evh, std::placeholders::_1);
	user_et->invoke<event_handler_int_t>(3, 3);

	assert(user_evh->call_count == 5);

	user_et->bind < event_handler_int_t>(4, std::bind(&fooo, std::placeholders::_1));
	user_et->invoke<event_handler_int_t>(4, 6);

	return 0;
}