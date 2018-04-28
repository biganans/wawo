
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

void spawn_new_timer(int);

void tick(WWRP<wawo::ref_base> const& cookie_, WWRP<wawo::timer> const& t ) {
	WWRP<cookie> c = wawo::static_pointer_cast<cookie>(cookie_);
	WAWO_INFO("i: %d, j: %d", c->i , c->j );

	if ( ++c->j > 5) {
		wawo::global_timer_manager::instance()->stop(t);
	}

	int i = std::rand();
	if (std::rand() < 2000) {
		spawn_new_timer(i);

		//WWRP<wawo::timer> t = wawo::make_ref<wawo::timer>(delay, true, c, &tick);
		//wawo::global_timer_manager::instance()->start(t);
	}
}

void spawn_new_timer(int i) {
	WWRP<cookie> c = wawo::make_ref<cookie>(i);
	WWRP<wawo::timer> t = wawo::make_ref<wawo::timer>(delay, true, c, &tick);
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

int main(int argc, char** argv) {

	//int* p = new int(2);
	/*
	while (1) {
		WWRP<cookie>* src = new WWRP<cookie>[2];

		src[0] = wawo::make_ref<cookie>(1);
		src[1] = wawo::make_ref<cookie>(2);

		
		WWRP<cookie>* tar = new WWRP<cookie>[2];
		tar[0] = std::move(src[0]);
		tar[1] = std::move(src[1]);
		

		delete[] src;
		delete[] tar;
	}
	*/
/*
	std::vector<int> vi;

	vi.push_back(11);
	vi.push_back(21);

	std::vector<foo> vf;

	foo f1 = foo(1);
	foo f2 = foo(1);

	vf.push_back(f1);
	vf.push_back(f2);
*/

	std::srand(0);
	wawo::app _app;

	for (int i = 0; i < 1024; ++i) {
		spawn_new_timer(i);
	}

	_app.run_for();
	

	return 0;
}