cmake_minimum_required(VERSION 3.22)
project(main LANGUAGES CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

# Minimum macOS version
set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0")

# Frameworks needed on macOS
set(MAC_FRAMEWORKS
    IOKit
    Carbon
    Cocoa
    QuartzCore
    AudioToolbox
    System
    OpenGL
)

set(LINUX_FRAMEWORKS

)

file(GLOB_RECURSE SOURCES "../src/*.cpp")

add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
add_link_options(-fsanitize=address)
add_compile_options(-O0 -g)

# Define the executable
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(main PRIVATE pcap)

find_package(morcules-swiftnet REQUIRED)
target_link_libraries(main PRIVATE swiftnet::swiftnet)

find_package(SQLite3 REQUIRED)
target_include_directories(main PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(main PRIVATE ${SQLite3_LIBRARIES})

find_package(wxWidgets CONFIG REQUIRED)
target_link_libraries(main PRIVATE wx::core wx::base)

if(APPLE)

    foreach(FRAMEWORK ${MAC_FRAMEWORKS})
        target_link_libraries(${PROJECT_NAME} PRIVATE "-framework ${FRAMEWORK}")
    endforeach()

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _FILE_OFFSET_BITS=64
        wxDEBUG_LEVEL=0
        WXUSINGDLL
        __WXMAC__
        __WXOSX__
        __WXOSX_COCOA__
    )

elseif(UNIX)

    foreach(FRAMEWORK ${LINUX_FRAMEWORKS})
        target_link_libraries(${PROJECT_NAME} PRIVATE "-framework ${FRAMEWORK}")
    endforeach()

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _FILE_OFFSET_BITS=64
        wxDEBUG_LEVEL=0
    )    

endif()
