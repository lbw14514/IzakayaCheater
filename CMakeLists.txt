cmake_minimum_required(VERSION 3.15)
project(IzakayaCheater VERSION 0.3.0 LANGUAGES CXX RC)

configure_file(config.h.in config.h)

find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(IzakayaCheater
    resource.rc
    main.cpp
    save_editor.cpp
    web_server.cpp
    webview_host.cpp)

target_include_directories(IzakayaCheater PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/third_party/webview2/include)

target_link_libraries(IzakayaCheater PRIVATE ${wxWidgets_LIBRARIES} ws2_32)

add_custom_command(TARGET IzakayaCheater POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/web
            $<TARGET_FILE_DIR:IzakayaCheater>/web
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/x64/WebView2Loader.dll
            $<TARGET_FILE_DIR:IzakayaCheater>/WebView2Loader.dll)

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
    target_compile_options(IzakayaCheater PRIVATE /W4 /utf-8)
else()
    target_compile_options(IzakayaCheater PRIVATE -Wall -Wextra)
endif()
