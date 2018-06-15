#ifndef _WAWO_THREAD_TICKER_HPP
#define _WAWO_THREAD_TICKER_HPP

#include <chrono>
#include <queue>

#include <wawo/thread.hpp>

namespace wawo {

	typedef std::function<void()> fn_ticker;
	template <u64_t precision_interval /*in nanoseconds*/, u8_t slot_max=32>
	class ticker {

	public:
		static const u64_t PRECISION_INTERVAL = precision_interval;

	private:
		struct ticker_slot :
			public wawo::ref_base
		{
			ticker_slot(WWSP<fn_ticker> const& f, u64_t const& interval):
				_fn(f),
				_fn_ptr(f.get()),
				_interval( std::chrono::nanoseconds(interval) ),
#ifdef _WW_STD_CHRONO_STEADY_CLOCK_EXTEND_SYSTEM_CLOCK
				_last_tick( std::chrono::system_clock::now() - std::chrono::nanoseconds(interval) )
#else
				_last_tick( std::chrono::steady_clock::now() - std::chrono::nanoseconds(interval) )
#endif
			{
			}

			WWSP<fn_ticker> _fn;
			fn_ticker* _fn_ptr;

			std::chrono::nanoseconds _interval;
#ifdef _WW_STD_CHRONO_STEADY_CLOCK_EXTEND_SYSTEM_CLOCK
			std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> _last_tick;
#else
			std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> _last_tick;
#endif
		};
		
		spin_mutex m_mutex;
		WWRP<ticker_slot> m_slots[slot_max];

		WWRP<thread> m_th;
		int m_total;
		bool m_th_break;

		void _run() {
			
			WAWO_ASSERT(m_th != NULL);
			while (1) {
				{
					lock_guard<spin_mutex> lg(m_mutex);
					if (m_total == 0) {
						m_th_break = true;
						break;
					}

#ifdef _WW_STD_CHRONO_STEADY_CLOCK_EXTEND_SYSTEM_CLOCK
					const std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> now = std::chrono::system_clock::now();
#else
					const std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> now = std::chrono::steady_clock::now();
#endif
					for (u8_t i = 0; i < m_total; ++i) {
						if ( (m_slots[i]->_fn_ptr != NULL) && (now>=(m_slots[i]->_last_tick+m_slots[i]->_interval)) ) {
							(*(m_slots[i]->_fn_ptr))();
							m_slots[i]->_last_tick = now;
						}
					}
				}

				wawo::this_thread::no_interrupt_nsleep(precision_interval);
			}
		}

		inline void _check_start() {
			if (m_th != NULL && m_th_break != true) {
				return;
			}
			m_th = wawo::make_ref<wawo::thread>();
			WAWO_ALLOC_CHECK(m_th, sizeof(wawo::thread));
			int rt = m_th->start(&ticker::_run, this);
			WAWO_CONDITION_CHECK(rt == wawo::OK);
			m_th_break = false;
		}
	public:
		ticker() : m_total(0), m_th_break(false) {
			for (int i = 0; i < slot_max; ++i) {
				m_slots[i] = NULL;
			}
		}
		~ticker() {
			{
				if( m_th != NULL ) {
					m_th->interrupt();
				}
			}
			lock_guard<spin_mutex> lg(m_mutex);
			for (int i = 0; i < slot_max; ++i) {
				m_slots[i] = NULL;
				m_total = 0;
			}
		}
		void schedule(WWSP<fn_ticker> const& fn, u64_t const& interval = 0/*in nano*/) {
			lock_guard<spin_mutex> lg(m_mutex);
			WAWO_ASSERT(fn != NULL);
			WAWO_ASSERT((interval == 0) ? true : (interval >= PRECISION_INTERVAL));

			WWRP<ticker_slot> slot = wawo::make_ref<ticker_slot>(fn, interval);

			WAWO_ASSERT(m_total < slot_max);
			WAWO_ASSERT(m_slots[m_total] == NULL);

			m_slots[m_total++] = slot;
			_check_start();
		}

		void deschedule(WWSP<fn_ticker> const& fn) {
			WAWO_ASSERT(fn != NULL);
			lock_guard<spin_mutex> lg(m_mutex);

			WAWO_ASSERT(m_total > 0);
			int hit_idx = -1;
			for (int i = 0; i < slot_max; ++i) {
				if (m_slots[i]->_fn == fn) {
					hit_idx = i;
					break;
				}
			}
			if (hit_idx != -1) {
				for (int i = hit_idx; i < slot_max - 1; ++i) {
					m_slots[i] = m_slots[i + 1];
				}
				m_slots[slot_max - 1] = NULL;
				--m_total;
			}
		}
	};

	typedef ticker<10>			_nano_ticker;
	typedef ticker<1000>		_micro_ticker;
	typedef ticker<1000000ULL>	_milli_ticker;

	class nano_ticker :
		public _nano_ticker,
		public wawo::singleton<nano_ticker>
	{};

	class micro_ticker :
		public _micro_ticker,
		public wawo::singleton<micro_ticker>
	{};

	class milli_ticker :
		public _milli_ticker,
		public wawo::singleton<milli_ticker>
	{};
}
#endif