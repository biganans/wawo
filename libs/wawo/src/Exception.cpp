#include <wawo/Exception.hpp>
#include <wawo/basic.hpp>

namespace wawo {

#ifdef WAWO_PLATFORM_POSIX
	void trace( char stack_buffer[], int const& s ) {
		int j,nptrs;
		int current_stack_fill_pos=0;

		#define BUFFER_SIZE 100

		char binary_name[256];

		void* buffer[BUFFER_SIZE];
		char** strings;
		nptrs = backtrace(buffer,BUFFER_SIZE);

		//printf("backtrace() return %d address\n", nptrs );

		strings = backtrace_symbols(buffer,nptrs);
		if( strings == NULL ) {
			perror("backtrace_symbol");
			exit(EXIT_FAILURE);
		}

		for(j=1;j<nptrs;j++) {

			int _address_begin = wawo::strpos(strings[j], (char*)"[");
			int _address_end = wawo::strpos(strings[j],(char*)"]");
			memcpy( (void*)&stack_buffer[current_stack_fill_pos], (void* const)(strings[j] + _address_begin), _address_end-_address_begin);
			current_stack_fill_pos += (_address_end-_address_begin);
			stack_buffer[current_stack_fill_pos++] = ']';
			stack_buffer[current_stack_fill_pos++] = ' ';

			int _f_begin = wawo::strpos(strings[j],(char*)"(");
			int _f_end = wawo::strpos(strings[j],(char*)")");
			memcpy( (void*)&stack_buffer[current_stack_fill_pos], (void* const)(strings[j] + _f_begin), _f_end-_f_begin);
			current_stack_fill_pos += (_f_end-_f_begin);
			stack_buffer[current_stack_fill_pos++] = ')';
			stack_buffer[current_stack_fill_pos++] = ' ';

			stack_buffer[current_stack_fill_pos++] = '\n';

			if( j == 1 ) {
				char const* ba = "binary: ";
				memcpy( (void*)&binary_name[0], (void* const)ba, strlen(ba) );
				memcpy( (void*)&binary_name[strlen(ba)], (void* const)(strings[j]), _f_begin);
				binary_name [strlen(ba)+_f_begin] = '\0';
			}
		}

		memcpy( (void*)&stack_buffer[current_stack_fill_pos], (void* const)(binary_name), strlen(binary_name) );
		current_stack_fill_pos += strlen(binary_name);

		stack_buffer[current_stack_fill_pos] = '\0';
		free(strings);
	}
#endif

}