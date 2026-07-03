cmake_minimum_required(VERSION 3.5.0)
project(IzakayaCheater VERSION 0.1.4 LANGUAGES C CXX)
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_SOURCE_DIR}/config.h")
set (${PROJECT_NAME}_VERSION_MAJOR 0)
set (${PROJECT_NAME}_VERSION_MINOR 1)

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

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")

#安装编译的文件
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION "." )


#拷贝单个文件至安装目录
install(FILES
        "${PROJECT_SOURCE_DIR}/README.md"
        "${PROJECT_SOURCE_DIR}/LICENSE"
        DESTINATION ".")

install(DIRECTORY
        "${PROJECT_SOURCE_DIR}/DLLS/"
        DESTINATION ".")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
include(CPack)
