set(source_dir "${PROJECT_SOURCE_DIR}/src")

add_executable(trajectory_check trajectory_check.cpp "${source_dir}/compute/ballistic_preset.cpp"
    "${source_dir}/compute/trajectory.cpp")
add_executable(camera_calibration_check camera_calibration_check.cpp
    "${source_dir}/compute/camera_calibration.cpp")
add_executable(calibration_capture_check calibration_capture_check.cpp
    "${source_dir}/media/calibration_capture.cpp" "${source_dir}/media/camera_sim.cpp"
    "${source_dir}/media/frame.cpp")
add_executable(ballistic_store_check ballistic_store_check.cpp
    "${source_dir}/config/ballistic_store.cpp")
add_executable(menu_check menu_check.cpp
    ${autoaimer_menu_sources}
    "${source_dir}/ui/hud.cpp" "${source_dir}/compute/ballistic_preset.cpp"
    "${source_dir}/compute/camera_calibration.cpp" "${source_dir}/compute/trajectory.cpp")
add_executable(menu_controller_check menu_controller_check.cpp
    "${source_dir}/menu_controller.cpp" "${source_dir}/media/calibration_capture.cpp"
    "${source_dir}/media/camera_sim.cpp" "${source_dir}/media/frame.cpp")
add_executable(media_check media_check.cpp
    "${source_dir}/media/camera_sim.cpp" "${source_dir}/media/frame.cpp" "${source_dir}/media/video.cpp"
    "${source_dir}/ui/hud.cpp")
add_executable(device_check device_check.cpp)
add_executable(display_check display_check.cpp "${source_dir}/media/display_gc9a01.cpp")
add_executable(display_worker_check display_worker_check.cpp "${source_dir}/media/display_worker.cpp")
target_link_libraries(display_worker_check PRIVATE Threads::Threads)
target_link_libraries(display_check PRIVATE Threads::Threads)
add_executable(ui_preview_check ui_preview_check.cpp
    "${source_dir}/media/display_gc9a01.cpp"
    "${source_dir}/media/camera_sim.cpp" "${source_dir}/media/frame.cpp" "${source_dir}/media/video.cpp"
    ${autoaimer_menu_sources}
    "${source_dir}/ui/hud.cpp" "${source_dir}/compute/ballistic_preset.cpp"
    "${source_dir}/compute/camera_calibration.cpp" "${source_dir}/compute/trajectory.cpp")
target_link_libraries(ui_preview_check PRIVATE Threads::Threads)

foreach(target IN ITEMS trajectory_check camera_calibration_check calibration_capture_check
    ballistic_store_check menu_check menu_controller_check media_check device_check ui_preview_check
    display_check display_worker_check)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_include_directories(${target} PRIVATE "${source_dir}" "${CMAKE_CURRENT_SOURCE_DIR}")
endforeach()

add_test(NAME trajectory_models COMMAND trajectory_check)
add_test(NAME camera_calibration COMMAND camera_calibration_check
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/camera-calibration.conf")
add_test(NAME calibration_capture COMMAND calibration_capture_check
    "${CMAKE_CURRENT_BINARY_DIR}/calibration-capture")
add_test(NAME ballistic_persistence COMMAND ballistic_store_check
    "${CMAKE_CURRENT_BINARY_DIR}/ballistic-presets-test.conf")
add_test(NAME menu_navigation COMMAND menu_check)
add_test(NAME menu_controller COMMAND menu_controller_check)
add_test(NAME media_adapters COMMAND media_check)
add_test(NAME input_and_sensors COMMAND device_check)
add_test(NAME gc9a01_display COMMAND display_check "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/gc9a01-init.txt")
add_test(NAME display_thread COMMAND display_worker_check)
set_tests_properties(display_thread PROPERTIES TIMEOUT 15)
# 只替换测试可执行文件中的 SPI 系统调用，不需要主机拥有 SPI 设备。
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(spi_linux_check spi_linux_check.cpp "${source_dir}/io/spi_linux.cpp")
    target_compile_features(spi_linux_check PRIVATE cxx_std_17)
    target_include_directories(spi_linux_check PRIVATE "${source_dir}")
    target_compile_options(spi_linux_check PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(spi_linux_check PRIVATE
        -Wl,--wrap=open -Wl,--wrap=close -Wl,--wrap=ioctl -Wl,--wrap=write)
    add_test(NAME linux_spi_adapter COMMAND spi_linux_check)
endif()
add_test(NAME hud_preview COMMAND ui_preview_check
    "${CMAKE_CURRENT_BINARY_DIR}/ui-preview.ppm" "${CMAKE_CURRENT_BINARY_DIR}/gc9a01-preview.ppm")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(ballistic_store_check PRIVATE stdc++fs)
    target_link_libraries(calibration_capture_check PRIVATE stdc++fs)
    target_link_libraries(menu_controller_check PRIVATE stdc++fs)
endif()
