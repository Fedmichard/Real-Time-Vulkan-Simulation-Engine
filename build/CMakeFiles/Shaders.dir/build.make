# CMAKE generated file: DO NOT EDIT!
# Generated by "MinGW Makefiles" Generator, CMake Version 3.30

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

SHELL = cmd.exe

# The CMake executable.
CMAKE_COMMAND = C:\msys64\mingw64\bin\cmake.exe

# The command to remove a file.
RM = C:\msys64\mingw64\bin\cmake.exe -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = C:\Users\franc\Documents\Personal_Projects\Real-Time-Vulkan-Simulation-Engine

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = C:\Users\franc\Documents\Personal_Projects\Real-Time-Vulkan-Simulation-Engine\build

# Utility rule file for Shaders.

# Include any custom commands dependencies for this target.
include CMakeFiles/Shaders.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/Shaders.dir/progress.make

CMakeFiles/Shaders: C:/Users/franc/Documents/Personal_Projects/Real-Time-Vulkan-Simulation-Engine/shaders/gradient.comp.spv

C:/Users/franc/Documents/Personal_Projects/Real-Time-Vulkan-Simulation-Engine/shaders/gradient.comp.spv: C:/Users/franc/Documents/Personal_Projects/Real-Time-Vulkan-Simulation-Engine/shaders/gradient.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=C:\Users\franc\Documents\Personal_Projects\Real-Time-Vulkan-Simulation-Engine\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating C:/Users/franc/Documents/Personal_Projects/Real-Time-Vulkan-Simulation-Engine/shaders/gradient.comp.spv"
	-V C:/Users/franc/Documents/Personal_Projects/Real-Time-Vulkan-Simulation-Engine/shaders/gradient.comp -o C:/Users/franc/Documents/Personal_Projects/Real-Time-Vulkan-Simulation-Engine/shaders/gradient.comp.spv

Shaders: CMakeFiles/Shaders
Shaders: C:/Users/franc/Documents/Personal_Projects/Real-Time-Vulkan-Simulation-Engine/shaders/gradient.comp.spv
Shaders: CMakeFiles/Shaders.dir/build.make
.PHONY : Shaders

# Rule to build all files generated by this target.
CMakeFiles/Shaders.dir/build: Shaders
.PHONY : CMakeFiles/Shaders.dir/build

CMakeFiles/Shaders.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles\Shaders.dir\cmake_clean.cmake
.PHONY : CMakeFiles/Shaders.dir/clean

CMakeFiles/Shaders.dir/depend:
	$(CMAKE_COMMAND) -E cmake_depends "MinGW Makefiles" C:\Users\franc\Documents\Personal_Projects\Real-Time-Vulkan-Simulation-Engine C:\Users\franc\Documents\Personal_Projects\Real-Time-Vulkan-Simulation-Engine C:\Users\franc\Documents\Personal_Projects\Real-Time-Vulkan-Simulation-Engine\build C:\Users\franc\Documents\Personal_Projects\Real-Time-Vulkan-Simulation-Engine\build C:\Users\franc\Documents\Personal_Projects\Real-Time-Vulkan-Simulation-Engine\build\CMakeFiles\Shaders.dir\DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/Shaders.dir/depend

