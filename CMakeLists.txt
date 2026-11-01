cmake_minimum_required(VERSION 3.28)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
project(obs-notifier VERSION 1.3.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "OBS Notifier currently requires Windows x64 and the MSVC toolchain.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(OBS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/.deps/obs-studio-33.0.1-sources" CACHE PATH "OBS development source tree")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/.deps/qt")
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
set(OBS_RELEASE_CANDIDATE 0)
set(OBS_BETA 0)
configure_file("${OBS_SOURCE_DIR}/libobs/obsconfig.h.in" "${CMAKE_BINARY_DIR}/generated/obsconfig.h")

foreach(library IN ITEMS obs obs-frontend-api)
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/${library}.lib"
        COMMAND "${CMAKE_AR}" /nologo /machine:x64
            "/def:${CMAKE_SOURCE_DIR}/cmake/${library}.def"
            "/out:${CMAKE_BINARY_DIR}/${library}.lib"
        DEPENDS "${CMAKE_SOURCE_DIR}/cmake/${library}.def" VERBATIM)
endforeach()
add_custom_target(obs-imports DEPENDS "${CMAKE_BINARY_DIR}/obs.lib" "${CMAKE_BINARY_DIR}/obs-frontend-api.lib")
add_library(obs-api INTERFACE)
target_include_directories(obs-api INTERFACE "${OBS_SOURCE_DIR}/libobs"
    "${OBS_SOURCE_DIR}/frontend/api" "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(obs-api INTERFACE "${CMAKE_BINARY_DIR}/obs.lib" "${CMAKE_BINARY_DIR}/obs-frontend-api.lib")

add_library(notifier-ui STATIC src/settings.cpp src/toast.cpp src/settings-dialog.cpp)
target_include_directories(notifier-ui PUBLIC src)
target_link_libraries(notifier-ui PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets user32 winmm)
target_compile_options(notifier-ui PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_compile_definitions(notifier-ui PUBLIC QT_NO_KEYWORDS)

configure_file(src/resources.rc.in "${CMAKE_BINARY_DIR}/generated/resources.rc" @ONLY)
set_source_files_properties("${CMAKE_BINARY_DIR}/generated/resources.rc" PROPERTIES
    OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/notification-chime.wav")
add_library(obs-notifier MODULE src/plugin.cpp src/shortcut-monitor.cpp "${CMAKE_BINARY_DIR}/generated/resources.rc")
add_dependencies(obs-notifier obs-imports)
target_link_libraries(obs-notifier PRIVATE notifier-ui obs-api)
target_compile_options(obs-notifier PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_compile_definitions(obs-notifier PRIVATE OBS_NOTIFIER_VERSION="${PROJECT_VERSION}")
set_target_properties(obs-notifier PROPERTIES PREFIX "")
install(TARGETS obs-notifier LIBRARY DESTINATION obs-plugins/64bit RUNTIME DESTINATION obs-plugins/64bit)
