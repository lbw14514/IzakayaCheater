cmake_minimum_required(VERSION 3.15)
project(IzakayaCheater VERSION 0.4.3 LANGUAGES C CXX RC)

configure_file(config.h.in config.h)

find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_library(minhook STATIC
    third_party/minhook/src/buffer.c
    third_party/minhook/src/hook.c
    third_party/minhook/src/trampoline.c
    third_party/minhook/src/hde/hde64.c)

target_include_directories(minhook PUBLIC
    ${PROJECT_SOURCE_DIR}/third_party/minhook/include)
target_include_directories(minhook PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/minhook/src
    ${PROJECT_SOURCE_DIR}/third_party/minhook/src/hde)

set_target_properties(minhook PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON)

if(MSVC)
    target_compile_options(minhook PRIVATE /W0)
else()
    target_compile_options(minhook PRIVATE -w)
endif()

add_library(IzakayaCheatCore SHARED
    core/dllmain.cpp
    core/features.cpp
    core/game_api.cpp
    core/il2cpp_api.cpp
    core/ipc_server.cpp
    core/log.cpp
    core/main_thread.cpp)

target_include_directories(IzakayaCheatCore PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/core)

target_link_libraries(IzakayaCheatCore PRIVATE minhook)

set_target_properties(IzakayaCheatCore PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    PREFIX ""
    OUTPUT_NAME "IzakayaCheatCore")

if(MINGW)
    target_link_options(IzakayaCheatCore PRIVATE -static -static-libgcc -static-libstdc++)
elseif(MSVC)
    target_compile_options(IzakayaCheatCore PRIVATE /W4 /utf-8)
endif()

if(NOT MSVC)
    target_compile_options(IzakayaCheatCore PRIVATE -Wall -Wextra)
endif()

add_executable(IzakayaCheater
    resource.rc
    json_lite.cpp
    main.cpp
    save_editor.cpp
    trainer_client.cpp
    trainer_injector.cpp
    web_server.cpp
    webview_host.cpp)

add_dependencies(IzakayaCheater IzakayaCheatCore)

target_include_directories(IzakayaCheater PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/third_party/webview2/include)

target_link_libraries(IzakayaCheater PRIVATE ${wxWidgets_LIBRARIES} ws2_32)

add_custom_command(TARGET IzakayaCheater POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/x64/WebView2Loader.dll
            $<TARGET_FILE_DIR:IzakayaCheater>/WebView2Loader.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:IzakayaCheatCore>
            $<TARGET_FILE_DIR:IzakayaCheater>/IzakayaCheatCore.dll)

add_custom_target(sync_web ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/web
            $<TARGET_FILE_DIR:IzakayaCheater>/web)
add_dependencies(sync_web IzakayaCheater)

if(MINGW)
    get_filename_component(MINGW_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(runtime_dll libwinpthread-1.dll libstdc++-6.dll libgcc_s_seh-1.dll)
        if(EXISTS "${MINGW_RUNTIME_DIR}/${runtime_dll}")
            add_custom_command(TARGET IzakayaCheater POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MINGW_RUNTIME_DIR}/${runtime_dll}"
                        "$<TARGET_FILE_DIR:IzakayaCheater>/${runtime_dll}")
        endif()
    endforeach()
endif()

set_target_properties(IzakayaCheater PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

if(MINGW)
    target_link_options(IzakayaCheater PRIVATE -mwindows -static-libgcc -static-libstdc++)
elseif(MSVC)
    set_target_properties(IzakayaCheater PROPERTIES WIN32_EXECUTABLE ON)
    target_link_options(IzakayaCheater PRIVATE /MANIFEST:NO)
endif()

if(MSVC)
    target_compile_options(IzakayaCheater PRIVATE /W4 /utf-8)
else()
    target_compile_options(IzakayaCheater PRIVATE -Wall -Wextra)
endif()
