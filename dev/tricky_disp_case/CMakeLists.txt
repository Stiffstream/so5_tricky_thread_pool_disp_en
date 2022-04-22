cmake_minimum_required(VERSION 3.19)

project(tricky_disp_case)

add_executable(tricky_disp_case main.cpp)

target_link_libraries(tricky_disp_case PRIVATE
	sobjectizer::StaticLib
	fmt::fmt)

