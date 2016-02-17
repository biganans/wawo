PRJ_PLATFORM 	:= linux
X86_64 			:= 1
PRJ_CONFIG 		:= release

ifeq ($(X86_64),1)
	PRJ_PLATFORM := $(PRJ_PLATFORM)64
else
	PRJ_PLATFORM := $(PRJ_PLATFORM)32
endif


#build_config could be [release|debug]
ifdef build
	PRJ_CONFIG=$(build)
endif

LIBPREFIX	= lib
LIBEXT		= a

comma:=,
empty:=
space:=$(empty) $(empty)

LIB_FULL_CONFIG := $(PRJ_PLATFORM)_$(PRJ_CONFIG)

LIBNAME		:= wawo

LIB_DIR := ../..
LIB_INCLUDE_DIR := $(LIB_DIR)/include
LIB_SOURCE_DIR := $(LIB_DIR)/src

LIB_BUILD_DIR	:= $(LIB_DIR)/build/$(LIB_FULL_CONFIG)
LIB_BIN_DIR	:= $(LIB_DIR)/bin/$(LIB_FULL_CONFIG)

ifndef $(O_EXT)
	O_EXT=o
endif

#LIB_CPP_FILES	= $(shell find $(SRC_DIR) -name *.cpp -not -name ".*" -not -name "jni_calls.cpp")
LIB_CPP_FILES	= $(shell find $(LIB_SOURCE_DIR) -name *.cpp -not -name ".*")

LIB_O_FILES	:= $(LIB_CPP_FILES:.cpp=.$(O_EXT))
LIB_O_FILES	:= $(foreach path,$(LIB_O_FILES), $(subst $(LIB_SOURCE_DIR)/,,$(path)))
LIB_O_FILES	:= $(addprefix $(LIB_BUILD_DIR)/,$(LIB_O_FILES))

LIB_FULL_NAME	= $(LIBPREFIX)$(LIBNAME).$(LIBEXT)
LIB_FULL_PATH_NAME	= $(LIB_BIN_DIR)/$(LIB_FULL_NAME)


DEFINES	:= $(DEFINES) __WAWO_DAY_DAY_UP__ PLATFORM_POSIX GCC LINUX64
CC_MISC	:= -c -std=c++11

ifeq ($(PRJ_CONFIG),debug)
	DEFINES:=$(DEFINES) DEBUG
	CC_MISC:=$(CC_MISC) -g -Wall -O0
else
	DEFINES:=$(DEFINES) RELEASE
	CC_MISC:=$(CC_MISC) -fexpensive-optimizations -O3
endif

ifeq ($(X86_64),1)
    CC_MISC:=$(CC_MISC) -m64
else
    CC_MISC:=$(CC_MISC) -m32
endif


DEFINES :=\
	$(foreach define,$(DEFINES), -D$(define))
INCLUDES:= \
	$(foreach include,$(LIB_INCLUDE_DIR), -I"$(include)")	
	
DEP_INCLUDE = ../../../boost/boost_1_57_0

DEP_INCLUDE_WITH_I :=\
	$(foreach include,$(DEP_INCLUDE), -I"$(include)")	
	
INCLUDES:= $(INCLUDES) $(DEP_INCLUDE_WITH_I)


wawo: all
all: precc $(LIB_FULL_PATH_NAME)

precc:
	@echo "-------------------------------"
	@echo "PRJ_CONFIG: $(PRJ_CONFIG)"
	@echo \*\* compiling...
	@echo "LIB_FULL_CONFIG: $(LIB_FULL_CONFIG)"
	@echo "LIB_BUILD_DIR" $(LIB_BUILD_DIR)
	@echo "LIB_BIN_DIR" $(LIB_BIN_DIR)
	@echo ""
	
	@echo "LIB_CPP_FILES: $(LIB_CPP_FILES)"
	@echo ""

	@echo "LIB_O_FILES: $(LIB_O_FILES)"
	@echo ""
	
	@echo $(CXX) $(DEFINES) $(INCLUDES) $(CC_MISC) {in} -o {out}
	@echo "-------------------------------"
	@echo ""


$(LIB_FULL_PATH_NAME): $(LIB_O_FILES)
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	@echo \*\* linking...
	$(AR) rs $@ $^

$(LIB_BUILD_DIR)/%.o : $(LIB_SOURCE_DIR)/%.cpp
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	@echo "------build cpp file begin------"
	@echo $(<F)
	@echo "------build cpp file end------"
	
# 	$(CXX) -lrt -lpthread -lstd -MMD -MP -MF $(DEFINES) $(INCLUDES) $(CC_MISC) $< -o $@
	$(CXX) -MMD -MP -MF $(DEFINES) $(INCLUDES) $(CC_MISC) $< -o $@

# Don't load dependencies when we're cleaning
# ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
#    -include $(LIB_O_FILES:.o=.d)
# endif

clean:
	rm -rf $(LIB_BUILD_DIR)
	rm -rf $(LIB_FULL_PATH_NAME)