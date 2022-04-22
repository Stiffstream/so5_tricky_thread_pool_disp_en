cmake_minimum_required(VERSION 3.19)

project(so5_tricky_thread_pool_disp_en)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOBJECTIZER_BUILD_STATIC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(so_5)
add_subdirectory(fmt)

add_subdirectory(adv_thread_pool_case)
add_subdirectory(tricky_disp_case)

