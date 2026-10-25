cmake_minimum_required(VERSION 3.19)
project(CodeEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()          # 打开 AUTOMOC

qt_add_executable(CodeEditor
    main.cpp
    theme.h            theme.cpp
    codeeditor.h       codeeditor.cpp
    theme.h theme.cpp
    codeeditor.h codeeditor.cpp
    linenumberarea.h linenumberarea.cpp
    codehighlighter.h codehighlighter.cpp
    bracketmatcher.h bracketmatcher.cpp
)

target_link_libraries(CodeEditor PRIVATE Qt6::Widgets)
target_link_libraries(CodeEditor PRIVATE Qt6::Core)
target_link_libraries(CodeEditor PRIVATE Qt6::Widgets)
