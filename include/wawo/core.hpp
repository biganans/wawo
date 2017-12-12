#ifndef _WAWO_CORE_H_
#define _WAWO_CORE_H_

// include custome override config value first
//#include "../../../../wawo_config/wawo_config.h"

//custom generic compiler , platform infos for wawo
#include <wawo/config/compiler.h>
#include <wawo/config/platform.h>

#include <cstddef>
#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include <atomic>

#include <wawo/macros.h>

#include <wawo/config/config.h>
#include <wawo/constants.h>

#include <wawo/basic.hpp>
#include <wawo/time/time.hpp>
#include <wawo/exception.hpp>
#include <wawo/memory.hpp>
#endif //end _CONFIG_WAWO_DEFAULT_CONFIG_H_