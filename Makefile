# ====== Basic Configuration ======
TARGET      ?= project
BUILD_DIR   ?= build
SRC_DIRS    ?= src cgp
CXX         ?= g++
CXXSTD      ?= c++17

# Header include directories
INC_DIRS    := . src cgp

# Preprocessor definitions (-D)
DEFINES     := SOLUTION                        # Enable solution code
DEFINES     += IMGUI_IMPL_OPENGL_LOADER_GLAD   # ImGui uses GLAD for OpenGL function loading
DEFINES     += CGP_OPENGL_3_3                  # Target OpenGL 3.3 (alternatives: CGP_OPENGL_4_1, 4_3, 4_6)
DEFINES     += CGP_ERROR_EXCEPTION             # Throw exceptions on CGP errors (instead of abort)
# DEFINES   += CGP_NO_DEBUG                    # Uncomment to disable CGP assertions

# ====== Sources / Objects / Dependencies ======
# Find .cpp and .c files
SRCS := $(shell find $(SRC_DIRS) -type f \( -name '*.cpp' -o -name '*.c' \))
# Put object files under build/ while keeping folder structure
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS)))
DEPS := $(OBJS:.o=.d)

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
.PHONY: all clean run
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Link  $@"
	$(CXX) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

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

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) -r $(BUILD_DIR) $(TARGET) imgui.ini $(DEPS)

-include $(DEPS)
