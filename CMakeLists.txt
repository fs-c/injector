cmake_minimum_required(VERSION 3.16)
project(injector CXX)

add_executable(injector
	src/injector.cpp)

target_include_directories(injector PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_features(injector PRIVATE cxx_std_17)
