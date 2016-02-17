#ifndef _WAWO_TASK_TASK_RUNNER_POOL_H_
#define _WAWO_TASK_TASK_RUNNER_POOL_H_


#include <string>
#include <list>

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>

#include <wawo/thread/Mutex.h>
#include <wawo/thread/Condition.h>
#include <wawo/task/TaskRunner.h>


namespace wawo { namespace task {

	class Task;
	typedef std::vector<TaskRunner*> TaskRunnerList;

	class TaskRunnerPool {
	public:

		TaskRunnerPool( std::string const& name, int max_runner );
		~TaskRunnerPool();
		
		void Init();
		void Deinit() ;
	
	public:
		int NewTaskRunner() ;
		int AvailableTaskRunnerCount();
		int WaitingTaskRunnerCount();

		uint32_t CancelTask( WAWO_REF_PTR<Task_Abstract> const& task ) ;
		uint32_t CancelAll();

		int AssignTask( WAWO_REF_PTR<Task_Abstract> const& task ) ;
		inline uint32_t CurrentTaskRunnerCount() { return m_task_runner_list.size(); }
		void SetMaxTaskRunner( uint32_t const& count );
		inline uint32_t const& GetMaxTaskRunner() {return m_max_runner_count;}
		inline uint32_t const& GetMaxTaskRunner() const {return m_max_runner_count;}

	private:
		Mutex m_mutex;
		bool m_is_running:1;

		uint32_t m_max_runner_count;
		TaskRunnerList m_task_runner_list ;
		std::string m_pool_name ;
		uint32_t m_last_runner_idx;
	} ;

	class SequenceTask;
	typedef std::vector<SequenceTaskRunner*> SequenceTaskRunnerList;
	class SequenceTaskRunnerPool {

	public:
		SequenceTaskRunnerPool( int concurrency_runner = 4 ) ;
		~SequenceTaskRunnerPool();

		void Init();
		void Deinit();

		int AssignTask( WAWO_REF_PTR<SequenceTask> const& task );
	private:

		Mutex m_mutex;
		bool m_is_running:1;
		int m_concurrency;

		SequenceTaskRunnerList m_task_runner_list;
	};

}}
#endif