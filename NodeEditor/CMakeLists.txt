cmake_minimum_required(VERSION 3.19)
project(NodeEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
qt_standard_project_setup()

# 目前只有入口 + 主题。后面每加一个类，就往这个清单里加一行。
qt_add_executable(NodeEditor
    main.cpp
    mainwindow.cpp
    Theme.h          Theme.cpp
    nodeview.h nodeview.cpp
    NodeItems/nodeitem.h NodeItems/nodeitem.cpp
    NodeItems/type.h NodeItems/type.cpp
    NodeItems/portitem.h NodeItems/portitem.cpp
    EdgeItems/edgeitem.h EdgeItems/edgeitem.cpp
    nodescene.h nodescene.cpp

)

target_link_libraries(NodeEditor PRIVATE Qt6::Widgets)
target_link_libraries(NodeEditor PRIVATE Qt6::OpenGLWidgets)
