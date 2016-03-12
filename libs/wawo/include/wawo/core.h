#ifndef _WAWO_CORE_H_
#define _WAWO_CORE_H_

// include custome override config value first
//#include "../../../../wawo_config/wawo_config.h"

//custom generic compiler , platform infos for wawo
#include <stddef.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include <wawo/config/compiler.h>
#include <wawo/config/platform.h>
#include <wawo/macros.h>

#include <wawo/config/config.h>
#include <wawo/constants.h>
#include <wawo/memory/memory.h>

#include <wawo/NonCopyable.hpp>


#include <wawo/basic.hpp>
#include <wawo/time/time.hpp>
#include <wawo/Exception.hpp>

#endif //end _CONFIG_WAWO_DEFAULT_CONFIG_H_