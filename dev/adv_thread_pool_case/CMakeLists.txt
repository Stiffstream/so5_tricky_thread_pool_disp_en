cmake_minimum_required(VERSION 3.19)

project(adv_thread_pool_case)

add_executable(adv_thread_pool_case main.cpp)

target_link_libraries(adv_thread_pool_case PRIVATE
	sobjectizer::StaticLib
	fmt::fmt)

