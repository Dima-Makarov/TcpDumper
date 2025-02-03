cmake_minimum_required (VERSION 3.10)
project(TcpDumper VERSION 1.0.3)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Curses REQUIRED)
CHECK_LIBRARY_EXISTS("${CURSES_NCURSES_LIBRARY}"
    nodelay "" CURSES_NCURSES_HAS_NODELAY)
if(NOT CURSES_NCURSES_HAS_NODELAY)
    find_library(CURSES_EXTRA_LIBRARY tinfo)
    CHECK_LIBRARY_EXISTS("${CURSES_EXTRA_LIBRARY}"
        nodelay "" CURSES_TINFO_HAS_NODELAY)
endif()
if(CURSES_EXTRA_LIBRARY)
    set(CURSES_LIBRARIES ${CURSES_LIBRARIES} ${CURSES_EXTRA_LIBRARY})
endif()

# main

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif ()

add_subdirectory(third-party)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
add_executable(${PROJECT_NAME} main.cpp src/imtui-impl-ncurses.cpp src/imtui-impl-text.cpp)
target_link_libraries(${PROJECT_NAME} imgui-for-imtui ${CURSES_LIBRARIES} /usr/lib/arm-linux-gnueabihf/libts.so)
target_include_directories(${PROJECT_NAME} PUBLIC include/)
