#ifndef _WAWO_CONTEXT_HPP
#define _WAWO_CONTEXT_HPP

#include <wawo/core.hpp>

namespace wawo { namespace context {

	typedef void*   fcontext_t;

	struct transfer_t {
		fcontext_t  fctx;
		void    *   data;
	};

	extern "C" transfer_t jump_fcontext(fcontext_t const to, void * vp);
	extern "C" fcontext_t make_fcontext(void * sp, size_t size, void(*fn)(transfer_t));
	extern "C" transfer_t ontop_fcontext(fcontext_t const to, void * vp, transfer_t(*fn)(transfer_t));

}}
#endif