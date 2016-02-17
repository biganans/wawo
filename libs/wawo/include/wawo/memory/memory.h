#ifndef _WAWO_MEMORY_MEMORY_H_
#define _WAWO_MEMORY_MEMORY_H_


#include <cstdlib>


#define WAWO_DELETE(PTR) {do { if( PTR != 0 ) {delete PTR; PTR=NULL;} } while(false);}
namespace wawo { namespace core {


}}

#endif
