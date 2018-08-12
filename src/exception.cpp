#include <wawo/exception.hpp>
#include <wawo/string.hpp>

namespace wawo {

#ifdef WAWO_PLATFORM_GNU
	void stack_trace(char stack_buffer[], u32_t const& s) {
		int j, nptrs;
		u32_t current_stack_fill_pos = 0;

#define BUFFER_SIZE 100

		char binary_name[256];

		void* buffer[BUFFER_SIZE];
		char** strings;
		nptrs = backtrace(buffer, BUFFER_SIZE);

		//printf("backtrace() return %d address\n", nptrs );

		strings = backtrace_symbols(buffer, nptrs);
		if (strings == NULL) {
			perror("backtrace_symbol");
			exit(EXIT_FAILURE);
		}

		for (j = 1; j < nptrs; j++) {

			int _address_begin = wawo::strpos(strings[j], (char*)"[");
			int _address_end = wawo::strpos(strings[j], (char*)"]");
			::memcpy((void*)&stack_buffer[current_stack_fill_pos], (void* const)(strings[j] + _address_begin), _address_end - _address_begin);
			current_stack_fill_pos += (_address_end - _address_begin);
			stack_buffer[current_stack_fill_pos++] = ']';
			stack_buffer[current_stack_fill_pos++] = ' ';

			int _f_begin = wawo::strpos(strings[j], (char*)"(");
			int _f_end = wawo::strpos(strings[j], (char*)")");
			::memcpy((void*)&stack_buffer[current_stack_fill_pos], (void* const)(strings[j] + _f_begin), _f_end - _f_begin);
			current_stack_fill_pos += (_f_end - _f_begin);
			stack_buffer[current_stack_fill_pos++] = ')';
			stack_buffer[current_stack_fill_pos++] = ' ';

			stack_buffer[current_stack_fill_pos++] = '\n';

			if (j == 1) {
				char const* ba = "binary: ";
				::memcpy((void*)&binary_name[0], (void* const)ba, strlen(ba));
				::memcpy((void*)&binary_name[strlen(ba)], (void* const)(strings[j]), _f_begin);
				binary_name[strlen(ba) + _f_begin] = '\0';
			}
		}

		::memcpy((void*)&stack_buffer[current_stack_fill_pos], (void* const)(binary_name), strlen(binary_name));
		current_stack_fill_pos += strlen(binary_name);

		stack_buffer[current_stack_fill_pos] = '\0';
		assert(current_stack_fill_pos <= s);
		::free(strings);
	}
#endif
}


#ifdef WAWO_PLATFORM_WIN
	#include "./../3rd/stack_walker/StackWalker.h"
#endif

#ifdef WAWO_PLATFORM_WIN
namespace wawo {
	class stack_walker : public StackWalker {
	public:
		std::string stack_info;
		stack_walker() : StackWalker(), stack_info() {
		}
	protected:
		virtual void OnOutput(LPCSTR szText)
		{
			stack_info += std::string(szText);
			StackWalker::OnOutput(szText);
		}
	};

	void stack_trace(char stack_buffer[], wawo::u32_t const& s) {
		stack_walker sw;
		sw.ShowCallstack();
		assert(sw.stack_info.length() > 0);

		if (sw.stack_info.length() > (s - 1)) {
			::memcpy(stack_buffer, sw.stack_info.c_str(), s - 1);
			stack_buffer[s-1] = '\0';
		}
		else {
			::memcpy(stack_buffer, sw.stack_info.c_str(), sw.stack_info.length());
			stack_buffer[sw.stack_info.length()] = '\0';
		}
	}
}
#endif

/*
#define __WAWO_EXCEPTION_INIT__(e, code_, sz_message_,sz_file_,line_, sz_func_, stack_info_ ) \
		e->code = code_; \
		if (sz_message_ != 0) { \
			int len_1 = ::strlen(sz_message_); \
			int len_2 = sizeof(e->message) / sizeof(e->message[0]) - 1; \
			int copy_len = len_1 > len_2 ? len_2 : len_1; \
			::memcpy(e->message, sz_message_, copy_len); \
			e->message[copy_len] = '\0'; \
		} \
		else { \
			e->message[0] = '\0'; \
		} \
		if (sz_file_ != 0) { \
			int len_1 = ::strlen(sz_file_); \
			int len_2 = sizeof(e->file) / sizeof(e->file[0]) - 1; \
			int copy_len = len_1 > len_2 ? len_2 : len_1; \
			::memcpy(e->file, sz_file_, copy_len); \
			e->file[copy_len] = '\0'; \
		} \
		else { \
			e->file[0] = '\0'; \
		} \
		e->line = line_; \
		if (sz_func_ != 0) { \
			int len_1 = ::strlen(sz_func_); \
			int len_2 = sizeof(e->func) / sizeof(e->func[0]) - 1; \
			int copy_len = len_1 > len_2 ? len_2 : len_1; \
			::memcpy(e->func, sz_func_, copy_len); \
			e->func[copy_len] = '\0'; \
		} \
		else { \
			e->func[0] = '\0'; \
		} \
		if (stack_info_ != 0 ) { \
			int info_len = ::strlen(stack_info_); \
			::memcpy(e->callstack, stack_info_, info_len ); \
			*((e->callstack) + info_len) = '\0'; \
		} \
		else { \
			e->callstack = 0; \
		} \
		*/
	
	void __WAWO_EXCEPTION_INIT__( wawo::exception* e, int code_, char const* const sz_message_, char const* const sz_file_, int line_, char const* const sz_func_, char const* const stack_info_ ) {
		e->code = code_;
		if (sz_message_ != 0) {
			wawo::size_t len_1 = ::strlen(sz_message_);
			wawo::size_t len_2 = sizeof(e->message) / sizeof(e->message[0]) - 1;
			wawo::size_t copy_len = len_1 > len_2 ? len_2 : len_1;
			::memcpy(e->message, sz_message_, copy_len); 
			e->message[copy_len] = '\0'; 
		} 
		else {
			e->message[0] = '\0'; 
		}
		if (sz_file_ != 0) {
			wawo::size_t len_1 = ::strlen(sz_file_); 
			wawo::size_t len_2 = sizeof(e->file) / sizeof(e->file[0]) - 1;
			wawo::size_t copy_len = len_1 > len_2 ? len_2 : len_1;
			::memcpy(e->file, sz_file_, copy_len); 
			e->file[copy_len] = '\0'; 
		} 
		else {
			e->file[0] = '\0'; 
		} 
		e->line = line_; 
		if (sz_func_ != 0) {
			wawo::size_t len_1 = ::strlen(sz_func_);
			wawo::size_t len_2 = sizeof(e->func) / sizeof(e->func[0]) - 1;
			wawo::size_t copy_len = len_1 > len_2 ? len_2 : len_1;
			::memcpy(e->func, sz_func_, copy_len); 
			e->func[copy_len] = '\0'; 
		} 
		else {
			e->func[0] = '\0'; 
		} 
		if (stack_info_ != 0) {
			if (e->callstack != 0) {
				::free(e->callstack);
			}

			wawo::size_t info_len = ::strlen(stack_info_);
			e->callstack = (char*)::malloc(info_len + 1);
			::memcpy(e->callstack, stack_info_, info_len);
			*((e->callstack) + info_len) = '\0'; 
		}
		else {
			e->callstack = 0; 
		} 
	}

namespace wawo {
	exception::exception(int const& code_, char const* const sz_message_, char const* const sz_file_, int const& line_, char const* const sz_func_, bool const& no_stack_info ) :
		code(code_),
		line(line_),
		callstack(0)
	{
		if (no_stack_info == false) {
			const u32_t info_size = 1024*64;
			char info[info_size] = {0};
			stack_trace(info, info_size);
			__WAWO_EXCEPTION_INIT__(this, code_, sz_message_, sz_file_, line_, sz_func_, info);
		}
		else {
			__WAWO_EXCEPTION_INIT__(this, code_, sz_message_, sz_file_, line_, sz_func_, callstack);
		}
	}

	exception::~exception() {
		if (callstack != 0) {
			::free(callstack);
			callstack = NULL;
		}
	}

	exception::exception(exception const& other) :
		code(other.code),
		line(other.line)
	{
		__WAWO_EXCEPTION_INIT__(this, other.code, other.message, other.file, other.line, other.func, other.callstack);
	}

	exception& exception::operator= (exception const& other) {
		__WAWO_EXCEPTION_INIT__(this, other.code, other.message, other.file, other.line, other.func, other.callstack);
		return *this;
	}
}