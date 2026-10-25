cmake_minimum_required(VERSION 3.19)
project(CodeEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()          # 打开 AUTOMOC

qt_add_executable(CodeEditor
    main.cpp
)

target_link_libraries(CodeEditor PRIVATE Qt6::Widgets)
