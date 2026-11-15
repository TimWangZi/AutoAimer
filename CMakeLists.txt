cmake_minimum_required(VERSION 3.16)
project(AutoAimer LANGUAGES CXX)

# std::thread 的系统链接要求由 CMake 检测，无需在应用中直接使用 pthread API。
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# 相机/RGA 与 SPI 屏幕是两个独立开关。SC3336 实机版使用 ON / ON。
option(AUTOAIMER_BOARD "Build for Luckfox Pico RV1103 with RKMPI and RGA" OFF)
option(AUTOAIMER_SPI_DISPLAY "Output to the seven-pin GC9A01 panel on Linux" OFF)

# 各页面独立实现；应用和菜单测试复用同一份源文件列表。
set(autoaimer_menu_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/level_indicator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu_stack.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menus/view_menu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menus/settings_menu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menus/ballistic_menu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menus/ballistic_parameter_menu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menus/parameter_menu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menus/calibration_menu.cpp")
add_executable(autoaimer
    src/main.cpp
    src/app.cpp
    src/menu_controller.cpp
    src/config/ballistic_store.cpp
    src/compute/ballistic_preset.cpp
    src/compute/camera_calibration.cpp
    src/compute/trajectory.cpp
    src/media/calibration_capture.cpp
    src/media/frame.cpp
    src/media/video.cpp
    src/media/display_worker.cpp
    src/ui/hud.cpp
    ${autoaimer_menu_sources})
target_link_libraries(autoaimer PRIVATE Threads::Threads)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    # Luckfox 的 GCC 8 将 C++17 filesystem 实现放在单独的库中。
    target_link_libraries(autoaimer PRIVATE stdc++fs)
endif()
target_compile_features(autoaimer PRIVATE cxx_std_17)
set_target_properties(autoaimer PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(autoaimer PRIVATE src)
target_compile_options(autoaimer PRIVATE -Wall -Wextra -Wpedantic)

# 输出端：Linux SPI/GPIO，或主机的空显示接口。
if(AUTOAIMER_SPI_DISPLAY)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "GC9A01 hardware output requires Linux spidev/GPIO")
    endif()
    target_sources(autoaimer PRIVATE src/media/display_gc9a01.cpp src/input/button_linux.cpp
        src/io/spi_linux.cpp src/io/gpio_linux.cpp)
    target_compile_definitions(autoaimer PRIVATE AUTOAIMER_SPI_DISPLAY)
endif()

# 输入端：SDK 相机 + RGA，或生成 NV12 图像的 camera_sim。
if(AUTOAIMER_BOARD)
    include(cmake/luckfox-media.cmake)
    target_sources(autoaimer PRIVATE src/media/camera_rv1103.cpp src/media/video_rga.cpp)
    target_compile_definitions(autoaimer PRIVATE AUTOAIMER_BOARD)
    target_link_libraries(autoaimer PRIVATE luckfox_media)
else()
    target_sources(autoaimer PRIVATE src/media/camera_sim.cpp)
    if(NOT AUTOAIMER_SPI_DISPLAY)
        set_target_properties(autoaimer PROPERTIES OUTPUT_NAME autoaimer_sim)
    endif()
    if(NOT CMAKE_CROSSCOMPILING)
        # 单元测试仅在主机构建中注册，不编入板端应用主循环。
        include(CTest)
        if(BUILD_TESTING)
            add_subdirectory(tests)
        endif()
    endif()
endif()
