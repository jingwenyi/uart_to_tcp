cmake_minimum_required( VERSION 2.8 )
project(link_model)

set( CMAKE_CXX_FLAGS "-std=c++11" )


add_executable(serial_to_network  serial_to_network.cpp )
target_link_libraries(serial_to_network  pthread)
