cmake_minimum_required (VERSION 2.8.11)
project (emp-agmpc)
set(NAME "emp-agmpc")

find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)

include(${CMAKE_FOLDER}/cmake/common.cmake)
include(${CMAKE_FOLDER}/cmake/enable_rdseed.cmake)
include(${CMAKE_FOLDER}/cmake/enable_float.cmake)

add_compile_options("-Wpointer-arith")

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)
add_compile_options(-fsanitize=pointer-compare)
add_link_options(-fsanitize=pointer-compare)

#add_compile_options(-fsanitize=thread)
#add_link_options(-fsanitize=thread)

add_compile_options(-fsanitize=undefined)
add_link_options(-fsanitize=undefined)
add_compile_options(-fsanitize-address-use-after-scope)
add_link_options(-fsanitize-address-use-after-scope)
#add_compile_options(-fstack-protector-all)
#add_link_options(-fstack-protector-all)



FIND_PACKAGE(emp-ot REQUIRED)
INCLUDE_DIRECTORIES(${EMP-OT_INCLUDE_DIRS})

install(DIRECTORY emp-agmpc DESTINATION include)

# Test cases
macro (add_test _name)
	add_test_with_lib(${_name} ${EMP-OT_LIBRARIES})
endmacro()

add_test (test_mpc)
add_test (test_mpc_individual)
add_test (aes)
add_test (sha1)
add_test (sha256)
add_test (triple)
