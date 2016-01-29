cmake_minimum_required(VERSION 2.6)
project(clingcon)

set(CMAKE_CXX_FLAGS "-std=c++11 -W -Wall")

add_subdirectory(liborder)
include_directories(${CMAKE_SOURCE_DIR}/clasp-3.2.x/libclasp ${CMAKE_SOURCE_DIR}/clasp-3.2.x/libprogram_opts ${CMAKE_SOURCE_DIR}/liblp/ ${CPPUNIT_INCLUDE_DIR})
add_definitions( -DWITH_THREADS=0 )
add_subdirectory(liblp)
add_subdirectory(clasp-3.2.x/libprogram_opts)
add_subdirectory(clasp-3.2.x/libclasp)
add_subdirectory(libclingcon)

add_subdirectory(app)
