# 使用 SDK 内与工具链 ABI 匹配的媒体库；这里只读取 SDK，不修改其内容。
set(media "${LUCKFOX_SDK_PATH}/media")
set(abi rv1106_arm-rockchip830-linux-uclibcgnueabihf)
set(rga "${media}/rga/release_rga_${abi}")
set(mpp "${media}/mpp/release_mpp_${abi}")
set(aiq "${media}/isp/release_camera_engine_rkaiq_${abi}")
set(rockit "${media}/rockit/rockit")
add_library(luckfox_media INTERFACE)
target_include_directories(luckfox_media SYSTEM INTERFACE
    "${rockit}/mpi/sdk/include" "${rga}/include/rga" "${aiq}/include/rkaiq"
    "${aiq}/include/rkaiq/common" "${aiq}/include/rkaiq/xcore"
    "${aiq}/include/rkaiq/algos" "${aiq}/include/rkaiq/iq_parser"
    "${aiq}/include/rkaiq/iq_parser_v2")
target_link_libraries(luckfox_media INTERFACE
    "${rockit}/lib/lib32/librockit.so" "${aiq}/lib/librkaiq.so"
    "${rga}/lib/librga.so" "${mpp}/lib/librockchip_mpp.so" pthread dl)
target_link_options(luckfox_media INTERFACE
    "-Wl,-rpath-link,${rockit}/lib/lib32:${aiq}/lib:${rga}/lib:${mpp}/lib")
set_target_properties(autoaimer PROPERTIES SKIP_BUILD_RPATH TRUE)
