# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/erobman/test_project/SOEM-master

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/erobman/test_project/SOEM-master/build

# Include any dependencies generated for this target.
include test/linux/simple_test/CMakeFiles/github_test.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/linux/simple_test/CMakeFiles/github_test.dir/compiler_depend.make

# Include the progress variables for this target.
include test/linux/simple_test/CMakeFiles/github_test.dir/progress.make

# Include the compile flags for this target's objects.
include test/linux/simple_test/CMakeFiles/github_test.dir/flags.make

test/linux/simple_test/CMakeFiles/github_test.dir/github.cpp.o: test/linux/simple_test/CMakeFiles/github_test.dir/flags.make
test/linux/simple_test/CMakeFiles/github_test.dir/github.cpp.o: ../test/linux/simple_test/github.cpp
test/linux/simple_test/CMakeFiles/github_test.dir/github.cpp.o: test/linux/simple_test/CMakeFiles/github_test.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/erobman/test_project/SOEM-master/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object test/linux/simple_test/CMakeFiles/github_test.dir/github.cpp.o"
	cd /home/erobman/test_project/SOEM-master/build/test/linux/simple_test && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT test/linux/simple_test/CMakeFiles/github_test.dir/github.cpp.o -MF CMakeFiles/github_test.dir/github.cpp.o.d -o CMakeFiles/github_test.dir/github.cpp.o -c /home/erobman/test_project/SOEM-master/test/linux/simple_test/github.cpp

test/linux/simple_test/CMakeFiles/github_test.dir/github.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/github_test.dir/github.cpp.i"
	cd /home/erobman/test_project/SOEM-master/build/test/linux/simple_test && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/erobman/test_project/SOEM-master/test/linux/simple_test/github.cpp > CMakeFiles/github_test.dir/github.cpp.i

test/linux/simple_test/CMakeFiles/github_test.dir/github.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/github_test.dir/github.cpp.s"
	cd /home/erobman/test_project/SOEM-master/build/test/linux/simple_test && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/erobman/test_project/SOEM-master/test/linux/simple_test/github.cpp -o CMakeFiles/github_test.dir/github.cpp.s

# Object files for target github_test
github_test_OBJECTS = \
"CMakeFiles/github_test.dir/github.cpp.o"

# External object files for target github_test
github_test_EXTERNAL_OBJECTS =

test/linux/simple_test/github_test: test/linux/simple_test/CMakeFiles/github_test.dir/github.cpp.o
test/linux/simple_test/github_test: test/linux/simple_test/CMakeFiles/github_test.dir/build.make
test/linux/simple_test/github_test: libsoem.a
test/linux/simple_test/github_test: test/linux/simple_test/CMakeFiles/github_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/erobman/test_project/SOEM-master/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable github_test"
	cd /home/erobman/test_project/SOEM-master/build/test/linux/simple_test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/github_test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/linux/simple_test/CMakeFiles/github_test.dir/build: test/linux/simple_test/github_test
.PHONY : test/linux/simple_test/CMakeFiles/github_test.dir/build

test/linux/simple_test/CMakeFiles/github_test.dir/clean:
	cd /home/erobman/test_project/SOEM-master/build/test/linux/simple_test && $(CMAKE_COMMAND) -P CMakeFiles/github_test.dir/cmake_clean.cmake
.PHONY : test/linux/simple_test/CMakeFiles/github_test.dir/clean

test/linux/simple_test/CMakeFiles/github_test.dir/depend:
	cd /home/erobman/test_project/SOEM-master/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/erobman/test_project/SOEM-master /home/erobman/test_project/SOEM-master/test/linux/simple_test /home/erobman/test_project/SOEM-master/build /home/erobman/test_project/SOEM-master/build/test/linux/simple_test /home/erobman/test_project/SOEM-master/build/test/linux/simple_test/CMakeFiles/github_test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/linux/simple_test/CMakeFiles/github_test.dir/depend

