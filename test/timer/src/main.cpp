

#include <wawo.h>
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
	if (std::rand() > 1000) {
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

int main(int argc, char** argv) {
	std::srand(0);
	wawo::app _app;

	for (int i = 0; i < 10240-1; ++i) {
		spawn_new_timer(i);
	}

	_app.run_for();
	return 0;
}