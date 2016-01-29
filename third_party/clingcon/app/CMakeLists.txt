file(GLOB APP_SRC main.cpp *.cpp clasp/*.cpp)
include_directories(${CMAKE_SOURCE_DIR}/libclasp ${CMAKE_SOURCE_DIR}/libprogram_opts  ${CMAKE_SOURCE_DIR}/liborder ${CMAKE_SOURCE_DIR}/libclingcon ${CMAKE_SOURCE_DIR}/app)
add_executable(clingcon_app ${APP_SRC})
target_link_libraries(clingcon_app clingcon clasp program_opts order lp)

set_target_properties(clingcon_app PROPERTIES OUTPUT_NAME clingcon)
SET( EXECUTABLE_OUTPUT_PATH  ${CMAKE_BINARY_DIR}/bin)

