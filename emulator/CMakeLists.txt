cmake_minimum_required(VERSION 3.16)

project(Emulator VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

file(GLOB_RECURSE PROJECT_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
)

file(GLOB FONT_FILES CONFIGURE_DEPENDS src/fonts/*.ttf)
set(FONT_QRC "${CMAKE_BINARY_DIR}/fonts.qrc")
set(FONT_QRC_CONTENT "<RCC><qresource prefix=\"/fonts\">\n")
foreach(font ${FONT_FILES})
    get_filename_component(font_name ${font} NAME)
    string(APPEND FONT_QRC_CONTENT "<file alias=\"${font_name}\">${font}</file>\n")
endforeach()
string(APPEND FONT_QRC_CONTENT "</qresource></RCC>\n")
file(WRITE ${FONT_QRC} "${FONT_QRC_CONTENT}")

qt_add_executable(Emulator
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    src/resources.qrc
    ${FONT_QRC}
)

target_compile_definitions(Emulator PRIVATE ORGAN_FONT_DIR="${CMAKE_SOURCE_DIR}/src/fonts")

target_include_directories(Emulator PUBLIC src)

target_link_libraries(Emulator PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui)

set_target_properties(Emulator PROPERTIES
    ${BUNDLE_ID_OPTION}
    WIN32_EXECUTABLE FALSE
)

qt_finalize_executable(Emulator)
