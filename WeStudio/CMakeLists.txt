cmake_minimum_required(VERSION 3.21)

project(WeStudio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Widgets
    Network
    Multimedia
    MultimediaWidgets
)

qt_standard_project_setup()

file(GLOB_RECURSE WES_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE WES_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

qt_add_executable(WeStudio
    ${WES_SOURCES}
    ${WES_HEADERS}
)

target_include_directories(WeStudio PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/workspaces/board"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/workspaces/code"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/workspaces/collab"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/workspaces/flow"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/workspaces/shot"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/workspaces/table"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/workspaces/timeline"
)

target_link_libraries(WeStudio PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

if(MSVC)
    target_compile_options(WeStudio PRIVATE /utf-8)
endif()

set_target_properties(WeStudio PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

