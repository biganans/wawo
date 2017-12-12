#ifndef _WAWO_TASK_TASK_HPP_
#define _WAWO_TASK_TASK_HPP_

#include <functional>
#include <vector>
#include <queue>

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>

namespace wawo { namespace task {

	// interface for task
	struct task_abstract:
		public wawo::ref_base
	{
		task_abstract() {}
		virtual ~task_abstract() {}
		virtual void run() = 0;
	};

	typedef void (*fn_task) ( WWRP<ref_base> const& cookie);
	class task :
		public task_abstract {

	private:
		fn_task m_run_func;
		WWRP<ref_base> m_cookie ;

	public:
		explicit task(fn_task const& run, WWRP<ref_base> const& cookie ):
			task_abstract(),
			m_run_func(run),
			m_cookie(cookie)
		{
		}

		~task() {}

		//for run
		void run() {
			WAWO_ASSERT(m_run_func != NULL );
			m_run_func( m_cookie ) ;
		}
	};

	typedef std::function<void ()> fn_lambda ;

	class lambda_task:
		public task_abstract {
		WAWO_DECLARE_NONCOPYABLE(lambda_task)
		fn_lambda m_run_fn ;
	public:
		explicit lambda_task( fn_lambda const& run) :
			task_abstract(),
			m_run_fn(run)
		{
		}

		void run() {
			WAWO_ASSERT(m_run_fn != nullptr );
			m_run_fn();
		}
	};


//#define DEBUG_SEQ

#ifdef DEBUG_SEQ
	static std::atomic<u64_t> s_seq_aid(1);
#endif

	class sequencial_task:
		public task_abstract
	{
		WAWO_DECLARE_NONCOPYABLE(sequencial_task)
		WWRP<task_abstract> m_task;
	public:
#ifdef DEBUG_SEQ
		u64_t m_id;
#endif
		u32_t m_tag;
		explicit sequencial_task( WWRP<task_abstract> const& task, u32_t const& tag ):
			m_task(task),
#ifdef DEBUG_SEQ
			m_id( wawo::atomic_increment(&s_seq_aid )),
#endif
			m_tag(tag)
		{
#ifdef DEBUG_SEQ
			WAWO_INFO("[sequencial_task]new seq task: %u", m_id);
#endif
		}

		~sequencial_task() {}

		void run() {
			WAWO_ASSERT( m_task != NULL );
#ifdef DEBUG_SEQ
			WAWO_INFO("[sequencial_task]seq task run: %u", m_id);
#endif
			m_task->run();
		}
	};
}}
#endif