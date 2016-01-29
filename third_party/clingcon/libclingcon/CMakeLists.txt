include_directories(clingcon)
include_directories(${CMAKE_SOURCE_DIR}/liborder/ ${CMAKE_SOURCE_DIR}/liblp/)
file(GLOB SOURCES src/*.cpp)
add_library(clingcon ${SOURCES})
