

#include <wawo.h>

struct cookie : wawo::ref_base
{
	WWRP<wawo::timer> _timer;
	int i;
	int j;
	cookie(int i_) :i(i_),j(0) {}
};

void tick(WWRP<wawo::ref_base> const& cookie_, WWRP<wawo::timer> const& t ) {
	WWRP<cookie> c = wawo::static_pointer_cast<cookie>(cookie_);
	WAWO_INFO("i: %d", c->i );

	++c->j;
	if (c->j > 1000) {
		wawo::timer_manager::instance()->stop(t);
	}
}

int main(int argc, char** argv) {

	wawo::app _app;
	std::chrono::nanoseconds delay = std::chrono::nanoseconds(1000 * 10); //1s

	for (int i = 0; i < 10240-1; ++i) {
		WWRP<cookie> c = wawo::make_ref<cookie>(i);
		WWRP<wawo::timer> t = wawo::make_ref<wawo::timer>(delay, true, c, &tick);
		wawo::timer_manager::instance()->start(t);
	}

	_app.run_for();
	return 0;
}