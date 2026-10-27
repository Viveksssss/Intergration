cmake_minimum_required(VERSION 3.19)
project(CodeEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()          # 打开 AUTOMOC

# ---- 本次新增的文件单独列成变量，方便「想退回原版时只删这几行」 ----
set(EXTRA_SOURCES
    fileexplorer.h     fileexplorer.cpp
    editortabs.h       editortabs.cpp
    fileops.h          fileops.cpp
)

qt_add_executable(CodeEditor WIN32
    app.rc
    main.cpp
    theme.h            theme.cpp
    codeeditor.h       codeeditor.cpp
    theme.h theme.cpp
    codeeditor.h codeeditor.cpp
    linenumberarea.h linenumberarea.cpp
    codehighlighter.h codehighlighter.cpp
    bracketmatcher.h bracketmatcher.cpp
    foldmanager.h foldmanager.cpp
    mainwindow.h mainwindow.cpp
    ${EXTRA_SOURCES}
)

target_link_libraries(CodeEditor PRIVATE Qt6::Widgets)
target_link_libraries(CodeEditor PRIVATE Qt6::Core)
target_link_libraries(CodeEditor PRIVATE Qt6::Widgets)
