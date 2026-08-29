cmake_minimum_required(VERSION 3.16)
project(bev-project LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(BUILD_BEV_APP   "Build the bev-app executable"        ON)
option(BUILD_BEV_TOOLS "Build calibration/dev tools"         ON)
option(BUILD_BEV_TESTS "Build unit/integration tests"        OFF)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui calib3d)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# bev-core: reusable BEV library. No knowledge of Yocto or app-level concerns.
# ---------------------------------------------------------------------------
add_library(bev-core SHARED
    src/Calibration.cpp
    src/PerspectiveTransformer.cpp
    src/BEVProcessor.cpp
    src/Stitcher.cpp
    src/HealthMonitor.cpp
)

target_include_directories(bev-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(bev-core
    PUBLIC
        ${OpenCV_LIBS}
        Threads::Threads
)

set_target_properties(bev-core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    POSITION_INDEPENDENT_CODE ON
)

target_compile_options(bev-core PRIVATE -Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
# bev-app: application layer. Contains NO BEV algorithm logic of its own.
# ---------------------------------------------------------------------------
if(BUILD_BEV_APP)
    add_executable(bev-app app/main.cpp)
    target_link_libraries(bev-app PRIVATE bev-core)
    target_compile_options(bev-app PRIVATE -Wall -Wextra -Wpedantic)
    install(TARGETS bev-app RUNTIME DESTINATION bin)
endif()

# ---------------------------------------------------------------------------
# Developer tools (interactive calibration point selector, etc.)
# ---------------------------------------------------------------------------
if(BUILD_BEV_TOOLS)
    add_executable(select_points tools/select_points.cpp)
    target_link_libraries(select_points PRIVATE bev-core)
    install(TARGETS select_points RUNTIME DESTINATION bin)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_BEV_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS bev-core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/bev DESTINATION include)
install(DIRECTORY config/ DESTINATION /etc/bev
    FILES_MATCHING PATTERN "*.yaml"
)

message(STATUS "bev-project: OpenCV ${OpenCV_VERSION} found at ${OpenCV_INCLUDE_DIRS}")
message(STATUS "bev-project: BUILD_BEV_APP=${BUILD_BEV_APP} BUILD_BEV_TOOLS=${BUILD_BEV_TOOLS} BUILD_BEV_TESTS=${BUILD_BEV_TESTS}")
