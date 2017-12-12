PRJ_ARCH		:= x86_64
PRJ_BUILD		:= release

#
# usage
# make build=debug arch=x86_32
# make build=release arch=x86_64
#
#


# x86_32, x86_64
ifdef arch
	PRJ_ARCH:=$(arch)
endif

#build_config could be [release|debug]
ifdef build
	PRJ_BUILD:=$(build)
endif

LIBPREFIX	= lib
LIBEXT		= a

comma:=,
empty:=
space:=$(empty) $(empty)

LIB_ARCH_BUILD := $(PRJ_ARCH)_$(PRJ_BUILD)

LIBNAME		:= wawo

LIB_DIR := ../..
LIB_INCLUDE_DIR := $(LIB_DIR)/include
LIB_SOURCE_DIR := $(LIB_DIR)/src

LIB_BUILD_DIR	:= $(LIB_DIR)/build/$(LIB_ARCH_BUILD)
LIB_BIN_DIR		:= $(LIB_DIR)/bin/$(LIB_ARCH_BUILD)


LIB_3RD_DIR				:= $(LIB_DIR)/3rd
LIB_3RD_CPP_BUILD_DIR	:= $(LIB_3RD_DIR)/build/cpp/$(LIB_ARCH_BUILD)
LIB_3RD_C_BUILD_DIR		:= $(LIB_3RD_DIR)/build/c/$(LIB_ARCH_BUILD)

ifndef $(O_EXT)
	O_EXT=o
endif

LIB_CPP_FILES	= $(shell find $(LIB_SOURCE_DIR) -name *.cpp -not -name ".*")
LIB_O_FILES	:= $(LIB_CPP_FILES:.cpp=.$(O_EXT))
LIB_O_FILES	:= $(foreach path,$(LIB_O_FILES), $(subst $(LIB_SOURCE_DIR)/,,$(path)))
LIB_O_FILES	:= $(addprefix $(LIB_BUILD_DIR)/,$(LIB_O_FILES))


LIB_3RD_CPP_FILES 	:= $(shell find $(LIB_3RD_DIR) -name *.cpp -not -name ".*")
LIB_3RD_CPP_O_FILES	:= $(LIB_3RD_CPP_FILES:.cpp=.$(O_EXT))
LIB_3RD_CPP_O_FILES	:= $(foreach path,$(LIB_3RD_CPP_O_FILES), $(subst $(LIB_3RD_DIR)/,,$(path)))
LIB_3RD_CPP_O_FILES := $(addprefix $(LIB_3RD_CPP_BUILD_DIR)/,$(LIB_3RD_CPP_O_FILES))

LIB_3RD_C_FILES 	:= $(shell find $(LIB_3RD_DIR) -name *.c -not -name ".*")
LIB_3RD_C_O_FILES	:= $(LIB_3RD_C_FILES:.c=.$(O_EXT))
LIB_3RD_C_O_FILES	:= $(foreach path,$(LIB_3RD_C_O_FILES), $(subst $(LIB_3RD_DIR)/,,$(path)))
LIB_3RD_C_O_FILES 	:= $(addprefix $(LIB_3RD_C_BUILD_DIR)/,$(LIB_3RD_C_O_FILES))


LIB_FULL_NAME		:= $(LIBPREFIX)$(LIBNAME).$(LIBEXT)
LIB_FULL_PATH_NAME	:= $(LIB_BIN_DIR)/$(LIB_FULL_NAME)


DEFINES	:= $(DEFINES) __WAWO__

CC_MISC		:= -c -rdynamic
CC_C11		:= -std=c++11

ifeq ($(PRJ_BUILD),debug)
	DEFINES:=$(DEFINES) DEBUG
	CC_MISC:=$(CC_MISC) -g -Wall -O0
else
	DEFINES:=$(DEFINES) RELEASE
	CC_MISC:=$(CC_MISC) -fexpensive-optimizations -O3
endif

ifeq ($(PRJ_ARCH),x86_64)
    CC_MISC:=$(CC_MISC) -m64
else
    CC_MISC:=$(CC_MISC) -m32
endif


DEFINES :=\
	$(foreach define,$(DEFINES), -D$(define))
INCLUDES:= \
	$(foreach include,$(LIB_INCLUDE_DIR), -I"$(include)")


wawo: all
all: precc $(LIB_FULL_PATH_NAME)

precc:
	@echo "-------------------------------"
	@echo \*\* compiling...
	@echo "LIB_ARCH_BUILD:	$(LIB_ARCH_BUILD)"
	@echo "LIB_BUILD_DIR":	$(LIB_BUILD_DIR)
	@echo "LIB_BIN_DIR":	$(LIB_BIN_DIR)
	@echo ""
	
	@echo "LIB_CPP_FILES: $(LIB_CPP_FILES)"
	@echo "LIB_O_FILES: $(LIB_O_FILES)"
	
	@echo ""
	@echo "LIB_3RD_CPP_FILES: $(LIB_3RD_CPP_FILES)"
	@echo "LIB_3RD_CPP_O_FILES: $(LIB_3RD_CPP_O_FILES)"
	
	@echo ""
	@echo "LIB_3RD_C_FILES: $(LIB_3RD_C_FILES)"
	@echo "LIB_3RD_C_O_FILES: $(LIB_3RD_C_O_FILES)"
	
	@echo ""
	
	@echo $(CXX) $(DEFINES) $(INCLUDES) $(CC_MISC) {in} -o {out}
	@echo "-------------------------------"
	@echo ""


$(LIB_FULL_PATH_NAME): $(LIB_O_FILES) $(LIB_3RD_CPP_O_FILES) $(LIB_3RD_C_O_FILES)
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
	
	$(CXX) -fPIC -MMD -MP -MF $(DEFINES) $(INCLUDES) $(CC_MISC)  $(CC_C11) $< -o $@

$(LIB_3RD_CPP_BUILD_DIR)/%.o : $(LIB_3RD_DIR)/%.cpp
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	@echo "------build 3rd cpp file begin------"
	@echo $(<F)
	@echo "------build 3rd cpp file end------"
	
	$(CXX) -fPIC -MMD -MP -MF $(DEFINES) $(INCLUDES) $(CC_MISC) $(CC_C11) $< -o $@

$(LIB_3RD_C_BUILD_DIR)/%.o : $(LIB_3RD_DIR)/%.c
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	@echo "------build 3rd c file begin------"
	@echo $(<F)
	@echo "------build 3rd c file end------"
	
	$(CXX) -fPIC -MMD -MP -MF $(DEFINES) $(INCLUDES) $(CC_MISC) $< -o $@
	
# Don't load dependencies when we're cleaning
# ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
#    -include $(LIB_O_FILES:.o=.d)
# endif

clean:
	rm -rf $(LIB_BUILD_DIR)
	rm -rf $(LIB_FULL_PATH_NAME)
	rm -rf $(LIB_3RD_CPP_BUILD_DIR)
	rm -rf $(LIB_3RD_C_BUILD_DIR)
