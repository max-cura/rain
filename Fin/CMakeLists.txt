cmake_minimum_required(VERSION 3.18)
project(Rain
        DESCRIPTION "Falling water"
        LANGUAGES CXX)

set(RAIN_CFG_ALLOC_TRACE_SUPPORT $<IF:$<CONFIG:Debug>,ON,OFF> CACHE BOOL "Support for allocation tracing for library debugging.")
set(RAIN_CFG_RUNTIME_CHECKING_SUPPORT $<IF:$<CONFIG:Debug>,ON,OFF> CACHE BOOL "Support for various runtime checks")
set(RAIN_CFG_HIDE_BUILDINFO OFF CACHE BOOL "Hide build information variables")

macro(RainSetupLibraryTarget _RAIN_TARGET)
    target_compile_options(${_RAIN_TARGET} PRIVATE ${RAIN_OPTIONS_CXX})
    target_link_options(${_RAIN_TARGET} PRIVATE ${RAIN_OPTIONS_LINK})
    target_include_directories(${_RAIN_TARGET} PUBLIC
            ..
            ${CMAKE_CURRENT_BINARY_DIR})
    set_target_properties(${_RAIN_TARGET}
            PROPERTIES
            CXX_STANDARD 20)
    target_compile_definitions(${_RAIN_TARGET}
            INTERFACE
            RAIN_BUILDINFO_LIBNAME_PARTIAL ${_RAIN_TARGET})
endmacro()

macro(RainSetupCCacheForTarget _RAIN_TARGET)
    set_target_properties(${_RAIN_TARGET}
            PROPERTIES
            RULE_LAUNCH_COMPILE ccache
            RULE_LAUNCH_LINK ccache)
endmacro()

configure_file(Rain/MachineConfig.h.in Rain/MachineConfig.h)
add_library(Rain SHARED Rain/Alloc/Alloc.h Rain/Alloc/Alloc.cc Rain/Compiler.h
        ${CMAKE_CURRENT_BINARY_DIR}/Rain/MachineConfig.h Rain/Config.h Rain/Math/Integers.h Rain/Math/Util.h Rain/Lang/Maybe.h Rain/Lang/Util.h Rain/Sync/Locks.h)

RainSetupLibraryTarget(Rain)
RainSetupCCacheForTarget(Rain)
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    target_compile_definitions(Rain PRIVATE
            RAIN_DEBUG)
endif ()

