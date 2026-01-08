cmake_minimum_required(VERSION 3.14)
project(CrSDKRestServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# Find required packages
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# Source files
set(REST_SERVER_SOURCES
    src/main.cpp
    src/server/RestServer.cpp
    src/server/WebSocketHandler.cpp
    src/server/MjpegStreamer.cpp
    src/camera/CameraManager.cpp
    src/camera/CameraDeviceWrapper.cpp
    src/api/ApiRouter.cpp
    src/api/JsonHelpers.cpp
    # GRBL/CNC module
    src/grbl/SerialPort.cpp
    src/grbl/GrblController.cpp
)

add_executable(${PROJECT_NAME} ${REST_SERVER_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external
        ${CMAKE_SOURCE_DIR}/app/CRSDK
        ${CMAKE_SOURCE_DIR}/app
        ${Boost_INCLUDE_DIRS}
)

# Find SDK libraries
set(CR_SDK_LIB_DIR ${CMAKE_SOURCE_DIR}/external/crsdk)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${CR_SDK_LIB_DIR}/libCr_Core.so
        Threads::Threads
        ${Boost_LIBRARIES}
        ${CMAKE_DL_LIBS}
)

# Compiler options
if(UNIX AND NOT APPLE)
    target_compile_options(${PROJECT_NAME}
        PRIVATE
            -fstack-protector-all
            -fsigned-char
            -Wall
            -Wextra
            -Wno-unknown-pragmas
            -Wno-unused-parameter
    )
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            BOOST_ASIO_NO_DEPRECATED
    )
endif()

# Set RPATH
set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

# Copy SDK libraries to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CR_SDK_LIB_DIR}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
