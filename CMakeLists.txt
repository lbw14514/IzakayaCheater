cmake_minimum_required(VERSION 3.15)
project(IzakayaCheater VERSION 0.2.3 LANGUAGES CXX)

configure_file(config.h.in config.h)

find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(IzakayaCheater
    main.cpp
    ui/ui.cpp
    MyMainFrame.cpp
    utils.cpp
    MyAboutDialog.cpp
    save_editor.cpp)

target_include_directories(IzakayaCheater PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR})

target_link_libraries(IzakayaCheater PRIVATE ${wxWidgets_LIBRARIES})

set_target_properties(IzakayaCheater PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

if(MINGW)
    target_link_options(IzakayaCheater PRIVATE -mwindows)
elseif(MSVC)
    set_target_properties(IzakayaCheater PROPERTIES WIN32_EXECUTABLE ON)
endif()

if(MSVC)
    target_compile_options(IzakayaCheater PRIVATE /W4)
else()
    target_compile_options(IzakayaCheater PRIVATE -Wall -Wextra)
endif()
