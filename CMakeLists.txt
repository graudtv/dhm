cmake_minimum_required(VERSION 3.19)

project(dpm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Boost REQUIRED COMPONENTS headers program_options)
find_package(helib REQUIRED)

include_directories(${Boost_INCLUDE_DIRS} include)

add_executable(client client/client.cpp)
add_executable(worker worker/worker.cpp)

target_link_libraries(worker ${Boost_LIBRARIES} helib)
target_link_libraries(client ${Boost_LIBRARIES} helib)
