cmake_minimum_required(VERSION 3.16)
project(visu VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCV REQUIRED)

add_executable(visu
        src/main.cpp
        src/MainWindow.h
        src/MainWindow.cpp
        src/ImageWidget.h
        src/ImageWidget.cpp
        src/DoubleThresholdDialog.h
        src/DoubleThresholdDialog.cpp
        src/ShadowCompressionDialog.h
        src/ShadowCompressionDialog.cpp
        src/BrowserWidget.cpp
        src/BrowserWidget.h
        src/ThumbLabel.h
        src/ThumbLabel.cpp
)

target_include_directories(visu PRIVATE
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(visu PRIVATE
        Qt6::Widgets
        ${OpenCV_LIBS}
)
