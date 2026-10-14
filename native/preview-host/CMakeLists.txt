cmake_minimum_required(VERSION 3.25)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Native Apple silicon")
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE STRING "macOS minimum")
project(VirusTIPreview VERSION 0.1.0 LANGUAGES C CXX OBJCXX)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_AUV2_BUILDS OFF CACHE BOOL "" FORCE)
set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/vst3sdk")
set(public_sdk_SOURCE_DIR "${SDK_ROOT}/public.sdk")
add_subdirectory("${SDK_ROOT}" sdk-build)
add_executable(VirusTIInterface MACOSX_BUNDLE main.mm
 "${SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm"
 "${SDK_ROOT}/public.sdk/source/vst/hosting/plugprovider.cpp"
 "${SDK_ROOT}/public.sdk/source/common/memorystream.cpp")
set_source_files_properties(main.mm "${SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm" PROPERTIES COMPILE_FLAGS "-fobjc-arc")
target_link_libraries(VirusTIInterface PRIVATE sdk_hosting "-framework Cocoa")
target_include_directories(VirusTIInterface PRIVATE "${SDK_ROOT}")
set(VTR_PLUGIN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../vst3/build/VST3/Release/Virus TI Rework Development.vst3" CACHE FILEPATH "Rebuilt plug-in")
target_compile_definitions(VirusTIInterface PRIVATE "VTR_PLUGIN_PATH=\"${VTR_PLUGIN_PATH}\"")
set(VTR_PREVIEW_OUTPUT "VirusTIInterface" CACHE STRING "Preview app filename")
set(VTR_PREVIEW_ID "org.virus-ti-rework.interface-preview" CACHE STRING "Preview bundle identifier")
set(VTR_PREVIEW_NAME "Virus TI Interface" CACHE STRING "Preview display name")
set_target_properties(VirusTIInterface PROPERTIES OUTPUT_NAME "${VTR_PREVIEW_OUTPUT}" MACOSX_BUNDLE_GUI_IDENTIFIER "${VTR_PREVIEW_ID}" MACOSX_BUNDLE_BUNDLE_NAME "${VTR_PREVIEW_NAME}" MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0" MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0" MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
enable_testing()
add_custom_command(TARGET VirusTIInterface POST_BUILD
 COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_DIR:VirusTIInterface>/Contents/Resources"
 COMMAND "${CMAKE_COMMAND}" -E copy_directory "${VTR_PLUGIN_PATH}" "$<TARGET_BUNDLE_DIR:VirusTIInterface>/Contents/Resources/Virus TI Rework Development.vst3"
 VERBATIM)
add_test(NAME actual_vst3_attach_and_edit COMMAND VirusTIInterface --self-test "$<TARGET_BUNDLE_DIR:VirusTIInterface>/Contents/Resources/Virus TI Rework Development.vst3")
