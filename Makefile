# Two applications: complete (app_complete/) and minimal (app_minimal/)
#   make            build both
#   make complete   build only the complete application (same for minimal)

# ====== Basic Configuration ======
APPS        := complete minimal
BUILD_DIR   ?= build
SHARED_DIRS ?= dynamic_skinning_source lib_helper cgp   # compiled once, linked in both applications
CXX         ?= g++
CXXSTD      ?= c++17

# Header include directories (the directory of each application is added for its own sources)
INC_DIRS    := . lib_helper cgp

# Preprocessor definitions (-D)
DEFINES     := SOLUTION                        # Enable solution code
DEFINES     += IMGUI_IMPL_OPENGL_LOADER_GLAD   # ImGui uses GLAD for OpenGL function loading
DEFINES     += CGP_OPENGL_3_3                  # Target OpenGL 3.3 (alternatives: CGP_OPENGL_4_1, 4_3, 4_6)
DEFINES     += CGP_ERROR_EXCEPTION             # Throw exceptions on CGP errors (instead of abort)
# DEFINES   += CGP_NO_DEBUG                    # Uncomment to disable CGP assertions

# ====== Sources / Objects / Dependencies ======
# Find .cpp and .c files, and put object files under build/ while keeping folder structure
objects_of = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(patsubst %.c,$(BUILD_DIR)/%.o,$(shell find $(1) -type f \( -name '*.cpp' -o -name '*.c' \) -not -path '*/test/*' -not -path '*/build/*')))
OBJS_SHARED   := $(call objects_of,$(SHARED_DIRS))
OBJS_COMPLETE := $(call objects_of,app_complete)
OBJS_MINIMAL  := $(call objects_of,app_minimal)
DEPS := $(patsubst %.o,%.d,$(OBJS_SHARED) $(OBJS_COMPLETE) $(OBJS_MINIMAL))

# ====== pkg-config: GLFW required (windowing library for OpenGL) ======
ifeq ($(shell pkg-config --exists glfw3 && echo yes),yes)
  GLFW_CFLAGS := $(shell pkg-config --cflags glfw3)
  GLFW_LIBS   := $(shell pkg-config --libs glfw3)
else
  $(error glfw3 not found via pkg-config. Please install it (brew/apt) or set GLFW_CFLAGS/GLFW_LIBS manually)
endif

# ====== Compiler/Linker Flags ======
CPPFLAGS += $(addprefix -I,$(INC_DIRS)) $(GLFW_CFLAGS) \
            $(addprefix -D,$(DEFINES)) -MMD -MP
CXXFLAGS += -std=$(CXXSTD) -g -O2 -Wall -Wextra -Wfatal-errors \
            -Wno-sign-compare -Wno-type-limits -Wno-pragmas
LDFLAGS  +=
LDLIBS   += $(GLFW_LIBS)

# System-specific libraries (dl for dynamic linking on Linux, m for math)
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  LDLIBS += -ldl -lm
endif
ifeq ($(UNAME_S),Darwin)
  LDLIBS += -lm
endif

# ====== Rules ======
.PHONY: all clean
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

all: $(APPS)

complete: $(OBJS_COMPLETE) $(OBJS_SHARED)
	@echo "Link  $@"
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

minimal: $(OBJS_MINIMAL) $(OBJS_SHARED)
	@echo "Link  $@"
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Sources of an application: its own headers (scene.hpp, environment.hpp) are found in its directory
$(BUILD_DIR)/app_complete/%.o: CPPFLAGS += -Iapp_complete
$(BUILD_DIR)/app_minimal/%.o:  CPPFLAGS += -Iapp_minimal

# Compile C++ sources
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "C++   $<"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile C sources
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "C     $<"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BUILD_DIR) $(APPS) imgui.ini $(DEPS)

-include $(DEPS)
