cmake_minimum_required(VERSION 3.16)

project(Emulator VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent)

file(GLOB_RECURSE PROJECT_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
)

qt_add_executable(Emulator
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    src/resources.qrc
)

target_include_directories(Emulator PUBLIC src)

target_link_libraries(Emulator PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent)

set_target_properties(Emulator PROPERTIES
    ${BUNDLE_ID_OPTION}
    WIN32_EXECUTABLE FALSE
)

qt_finalize_executable(Emulator)

