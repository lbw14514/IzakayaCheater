cmake_minimum_required(VERSION 3.5.0)
project(IzakayaCheater VERSION 0.2.0 LANGUAGES C CXX)
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_SOURCE_DIR}/config.h")

include(CTest)
enable_testing()

set(SRCS main.cpp ui/ui.cpp MyMainFrame.cpp utils.cpp MyAboutDialog.cpp save_editor.cpp)

add_executable(IzakayaCheater ${SRCS})

find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base net gl)
if(wxWidgets_FOUND)
    include(${wxWidgets_USE_FILE})
    target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})
endif()

set(CMAKE_CXX_FLAGS "-mwindows -std=c++11 ${CMAKE_CXX_FLAGS}")
