cmake_minimum_required(VERSION 3.16)
project(visu VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


if(CMAKE_COMPILER_IS_GNUCXX)
    SET(GCC_COVERAGE_COMPILE_FLAGS "-Werror=return-type")
    add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
endif()

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
        src/ThumbWidget.cpp
        src/ThumbWidget.h
        src/DirOnlyModel.h
        src/avif_opencv.cpp
        src/avif_opencv.h
        src/jpeg_lossless.cpp
        src/jpeg_lossless.h
        src/ImageFormats.h
        src/SaveAsDialog.cpp
        src/SaveAsDialog.h
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVIF REQUIRED libavif)
find_path(TURBOJPEG_INCLUDE_DIR turbojpeg.h
        PATHS /usr/include /usr/local/include
)
find_library(TURBOJPEG_LIBRARY turbojpeg
        PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
)

if (NOT TURBOJPEG_INCLUDE_DIR OR NOT TURBOJPEG_LIBRARY)
    message(FATAL_ERROR "TurboJPEG not found")
endif()

target_include_directories(visu PRIVATE
        ${OpenCV_INCLUDE_DIRS}
        ${AVIF_INCLUDE_DIRS}
        ${TURBOJPEG_INCLUDE_DIR}
)

target_link_libraries(visu PRIVATE
        Qt6::Widgets
        ${OpenCV_LIBS}
        ${AVIF_LIBRARIES}
        ${TURBOJPEG_LIBRARY}
)
