cmake_minimum_required(VERSION 3.19)
project(TimelineDemo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 阶段 1 只需要 Widgets，等接视频时再加 Multimedia
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS MultimediaWidgets)
qt_standard_project_setup()

qt_add_executable(TimelineDemo main.cpp
    timelinewidget.h timelinewidget.cpp
    Theme.h
    TimeUtils.h
    TimeUtils.h
    mainwindow.h mainwindow.cpp
    Theme.cpp
)

target_link_libraries(TimelineDemo PRIVATE Qt6::Widgets)
target_link_libraries(TimelineDemo PRIVATE Qt6::Core Qt6::Widgets)
target_link_libraries(TimelineDemo PRIVATE Qt6::Widgets)
target_link_libraries(TimelineDemo PRIVATE Qt6::MultimediaWidgets)
