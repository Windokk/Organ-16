cmake_minimum_required(VERSION 3.16)

project(Emulator VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent)

set(PROJECT_SOURCES
        src/main.cpp
        src/layouts/io_ports.cpp
        src/layouts/screen/canvas.cpp
        src/layouts/regs/flow_layout.cpp
        src/layouts/ram_panel.cpp
        src/backend/cpu.cpp
        src/backend/clock.cpp
        src/backend/control_unit/control_unit.cpp
        src/backend/alu/alu.cpp
        src/backend/memory/memory_interface.cpp
        src/backend/memory/ram.cpp
        src/backend/registers/registers.cpp
        src/backend/temp_values/temp_values.cpp
)

qt_add_executable(Emulator
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    src/resources.qrc
)

target_link_libraries(Emulator PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent)

set_target_properties(Emulator PROPERTIES
    ${BUNDLE_ID_OPTION}
    WIN32_EXECUTABLE FALSE
)

qt_finalize_executable(Emulator)

