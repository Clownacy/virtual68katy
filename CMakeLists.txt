cmake_minimum_required(VERSION 3.1)

project(virtual68katy)

add_executable(virtual68katy
	"main.c"
	"thread.c"
	"thread.h"
	"clown68000/error.c"
	"clown68000/error.h"
	"clown68000/m68k.c"
	"clown68000/m68k.h"
	"clown68000/m68k/opcode.c"
	"clown68000/m68k/opcode.h"
)

set_target_properties(virtual68katy PROPERTIES
	C_STANDARD 90
	C_STANDARD_REQUIRED OFF
)

if(NOT WIN32)
	# POSIX platforms use POSIX threads.
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
	target_link_libraries(virtual68katy PRIVATE Threads::Threads)
endif()
