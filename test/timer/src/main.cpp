
#include <vld.h>

#include <wawo.h>
#include <chrono>
#include <vector>

std::chrono::nanoseconds delay = std::chrono::nanoseconds(1000 * 10); //1s

struct cookie : wawo::ref_base
{
	WWRP<wawo::timer> _timer;
	int i;
	int j;
	cookie(int i_) :i(i_),j(0) {}
};

void spawn_repeat_timer(int);

void repeat_timer_tick(WWRP<wawo::ref_base> const& cookie_, WWRP<wawo::timer> const& t ) {
	WWRP<cookie> c = wawo::static_pointer_cast<cookie>(cookie_);
	WAWO_INFO("i: %d, j: %d", c->i , c->j );

	if ( ++c->j>2) {
		wawo::global_timer_manager::instance()->stop(t);
	}

	//int i = std::rand();
	//if (std::rand() < 500 ) {
	//	spawn_repeat_timer(i);
	//}
}

void spawn_repeat_timer(int i) {
	WWRP<cookie> c = wawo::make_ref<cookie>(i);
	WWRP<wawo::timer> t = wawo::make_ref<wawo::timer>(delay, true, c, &repeat_timer_tick);
	wawo::global_timer_manager::instance()->start(t);
}

void spawn_user_circle_timer(int i);
void user_circle_tick(WWRP<wawo::ref_base> const& cookie_, WWRP<wawo::timer> const& t) {
	WWRP<cookie> c = wawo::static_pointer_cast<cookie>(cookie_);

	WAWO_INFO("i: %d, j: %d", c->i, c->j);
	//if (c->i == 1000) {
	//	wawo::signal::signal_manager::instance()->raise_signal(SIGINT);
	//}

	if (std::rand() & 0x01) {
		spawn_user_circle_timer(++c->i);
	}
}

void spawn_user_circle_timer(int i) {
	WWRP<cookie> c = wawo::make_ref<cookie>(i);
	WWRP<wawo::timer> t = wawo::make_ref<wawo::timer>(delay, c, &user_circle_tick);
	wawo::global_timer_manager::instance()->start(t);
}


void spawn_timer(int);
void timer_tick(WWRP<wawo::ref_base> const& cookie_, WWRP<wawo::timer> const& t) {
	WWRP<cookie> c = wawo::static_pointer_cast<cookie>(cookie_);
	WAWO_INFO("i: %d, j: %d", c->i, c->j);
}
void spawn_timer(int i) {
	WWRP<cookie> c = wawo::make_ref<cookie>(i);
	WWRP<wawo::timer> t = wawo::make_ref<wawo::timer>(delay, c, &timer_tick);
	wawo::global_timer_manager::instance()->start(t);
}
struct foo
{
	int i;
	foo(int i_):i(i_)
	{
		printf("__construct foo\n");
	}

	foo(const foo& o) : i(o.i)
	{
		printf("initialize foo\n");
	}

	foo(foo&& o):i(o.i)
	{
		printf("right v ref foo\n");
	}

	~foo()
	{
		printf("~foo: %d\n", i);
	}
};

void th_spawn_timer() {
	while (1) {
		int i = std::rand();
		if (i %3 == 0 ) {
			spawn_timer(i);
		}
		else if (i % 2 == 0) {
			spawn_user_circle_timer(i);
		}
		else {
			spawn_repeat_timer(i);
		}

		wawo::this_thread::usleep(1);
	}
}

int main(int argc, char** argv) {

	std::srand(0);
	wawo::app _app;

	/*
	for (int i = 0; i < 1024; ++i) {
		spawn_new_timer(i);
	}
	*/
	const int th_count = 8;
	WWRP<wawo::thread::thread> th[th_count];
	for (int i = 0; i < th_count; ++i) {
		th[i] = wawo::make_ref<wawo::thread::thread>();
		th[i]->start(&th_spawn_timer);
	}

	//spawn_circle_tick(1);
	_app.run_for();

	for (int i = 0; i < th_count; ++i) {
		th[i]->interrupt();
		th[i]->join();
	}

	return 0;
}