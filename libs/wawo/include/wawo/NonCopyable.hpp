#ifndef _WAWO_NON_COPYABLE_HPP_
#define _WAWO_NON_COPYABLE_HPP_

namespace wawo {
	class NonCopyable {
	protected:
		NonCopyable() {}
		~NonCopyable() {}
	private:
		NonCopyable( NonCopyable const& );
		NonCopyable& operator=( NonCopyable const& ) ;
	};
}
#endif