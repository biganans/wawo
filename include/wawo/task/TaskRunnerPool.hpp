#ifndef _WAWO_TASK_TASK_RUNNER_POOL_HPP_
#define _WAWO_TASK_TASK_RUNNER_POOL_HPP_


#include <string>
#include <list>

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>

#include <wawo/thread/Mutex.hpp>
#include <wawo/thread/Condition.hpp>
#include <wawo/task/TaskRunner.hpp>

namespace wawo { namespace task {

	typedef std::vector<TaskRunner*> TRV;

	class TaskRunnerPool {
	public:
		TaskRunnerPool( int const& max_runner = 4 );
		~TaskRunnerPool();

		void Init();
		void Deinit() ;

	public:
		inline int AssignTask( WWRP<Task_Abstract> const& task ) {
			WAWO_ASSERT( m_is_running == true );
			u32_t runner_size = m_runners.size();
			WAWO_ASSERT( m_last_runner_idx <= runner_size );

			if( m_last_runner_idx == runner_size ) {
				m_last_runner_idx=0;
			}

			while( m_last_runner_idx != runner_size ) {
				if( (m_runners[m_last_runner_idx])->IsWaiting() ) {
					if((m_runners[m_last_runner_idx])->AssignTask( task ) == wawo::OK) {
						return wawo::OK;
					}
				}
				++m_last_runner_idx;
			}

			if ( runner_size < m_max_concurrency ) {
				if ( _NewTaskRunner() == wawo::OK ) {
					return m_runners.back()->AssignTask( task ) ;
				}
			}

			return wawo::E_NO_TASK_RUNNER_AVAILABLE;
		}

		inline u32_t CurrentTaskRunnerCount() const { return m_runners.size(); }
		void SetMaxTaskRunner( u32_t const& count );
		inline u32_t const& GetMaxTaskRunner() const {return m_max_concurrency;}

	private:
		int _NewTaskRunner() ;

		Mutex m_mutex;
		bool m_is_running:1;

		u32_t m_max_concurrency;
		TRV m_runners ;
		u32_t m_last_runner_idx;
	} ;

	class SequencialTask;
	typedef std::vector<SequencialTaskRunner*> SequenceTaskRunnerList;

	class SequencialTaskRunnerPool {

	public:
		SequencialTaskRunnerPool( u32_t const& concurrency = 2 ) ;
		~SequencialTaskRunnerPool();

		inline int AssignTask( WWRP<SequencialTask> const& task ) const {
			WAWO_ASSERT( m_is_running == true );
			WAWO_ASSERT( task != NULL );
			WAWO_ASSERT( task->Isset() );

			u32_t ridx = (task->GetTag()%m_concurrency) ;
			WAWO_ASSERT( m_runners[ridx] != NULL );
			return m_runners[ridx]->AssignTask( task );
		}

		u32_t CancelAll() const {
			u32_t total =0;
			std::for_each( m_runners.begin(), m_runners.end(), [&total]( SequencialTaskRunner* const& str ) {
				total += str->CancelAll();
			});
			return total;
		}

		void Init();
		void Deinit();

	private:
		Mutex m_mutex;
		bool m_is_running:1;
		u32_t m_concurrency;

		SequenceTaskRunnerList m_runners;
	};
}}
#endif
