cmake_minimum_required(VERSION 3.31)
project(mesi_polo LANGUAGES CXX)

set(CMAKE_TOOLCHAIN_FILE "C:/users/aaron/D/dev/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(rapidfuzz_DIR "C:/users/aaron/D/dev/vcpkg/installed/x64-mingw-dynamic/share/rapidfuzz")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(rapidfuzz CONFIG REQUIRED)

qt_standard_project_setup()

qt_add_executable(mesi_polo
    WIN32 MACOSX_BUNDLE   
    widget.ui
    widget.h
    ConlangDictionary.h
    ConlangDictionary.cpp
    main.cpp
    widget.cpp
)

target_link_libraries(mesi_polo
    PRIVATE
        Qt::Core
        Qt::Widgets
        rapidfuzz::rapidfuzz
)

target_include_directories(mesi_polo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GNUInstallDirs)

install(TARGETS mesi_polo
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET mesi_polo
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
