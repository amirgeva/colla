cmake_minimum_required(VERSION 3.16)
project(colla)

option(X86 "Compile for 32bits" ON)

if (X86)
	set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-m32")
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-m32")
endif (X86)

option(DIAGNOSTICS "Enable full unit test functionality" OFF)
if (DIAGNOSTICS)
    add_definitions(-D_DIAGNOSTICS_)
endif(DIAGNOSTICS)

add_library(colla STATIC 
	src/colla.c
	include/colla.h
)
target_include_directories(colla PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(unittest src/unittest.c)
target_link_libraries(unittest PRIVATE colla)
