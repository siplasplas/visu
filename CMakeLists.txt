cmake_minimum_required(VERSION 3.16)
project(visu VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Get git commit SHA
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_SHA)
    set(GIT_SHA "unknown")
endif()

# Configure version header
configure_file(
    ${CMAKE_SOURCE_DIR}/src/config.h.in
    ${CMAKE_BINARY_DIR}/config.h
    @ONLY
)

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
        src/image_metrics.cpp
        src/image_metrics.h
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
        ${CMAKE_BINARY_DIR}
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

# Installation
include(GNUInstallDirs)

# Install executable
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install .desktop file
install(FILES res/visu.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Install SVG icon (scalable)
install(FILES res/icons/visu.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME visu.svg)

# Generate and install PNG icons from SVG at install time
find_program(INKSCAPE_EXECUTABLE inkscape)
find_program(RSVG_CONVERT_EXECUTABLE rsvg-convert)

if(INKSCAPE_EXECUTABLE)
    set(SVG_CONVERTER "${INKSCAPE_EXECUTABLE}")
elseif(RSVG_CONVERT_EXECUTABLE)
    set(SVG_CONVERTER "${RSVG_CONVERT_EXECUTABLE}")
endif()

if(SVG_CONVERTER)
    foreach(size 16 24 32 48 64 128 256)
        install(CODE "
            set(ICON_DIR \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps\")
            file(MAKE_DIRECTORY \"\${ICON_DIR}\")
            set(OUTPUT_FILE \"\${ICON_DIR}/visu.png\")
            set(INPUT_FILE \"${CMAKE_CURRENT_SOURCE_DIR}/res/icons/visu.svg\")
            if(\"${INKSCAPE_EXECUTABLE}\" STREQUAL \"${SVG_CONVERTER}\")
                execute_process(
                    COMMAND ${SVG_CONVERTER} \"\${INPUT_FILE}\" --export-type=png --export-filename=\${OUTPUT_FILE} --export-width=${size} --export-height=${size}
                    RESULT_VARIABLE result
                )
            else()
                execute_process(
                    COMMAND ${SVG_CONVERTER} -w ${size} -h ${size} -o \"\${OUTPUT_FILE}\" \"\${INPUT_FILE}\"
                    RESULT_VARIABLE result
                )
            endif()
            if(result EQUAL 0)
                message(STATUS \"Generated: \${OUTPUT_FILE}\")
            else()
                message(WARNING \"Failed to generate: \${OUTPUT_FILE}\")
            endif()
        ")
    endforeach()
else()
    message(WARNING "No SVG converter found (inkscape or rsvg-convert). PNG icons will not be installed.")
endif()
