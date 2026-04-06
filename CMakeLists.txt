cmake_minimum_required(VERSION 3.28...3.30)

# ------------------------------------------------------------------------------
# Bootstrap / project setup
# ------------------------------------------------------------------------------

# Load shared bootstrap logic that defines common project metadata such as
# `_name` and `_version`.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

# Define the project using values established during bootstrap.
project(${_name} VERSION ${_version})

# ------------------------------------------------------------------------------
# Build options
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# Xcode compatibility
# ------------------------------------------------------------------------------

if(CMAKE_GENERATOR STREQUAL "Xcode")
    # Recent Xcode versions can fail with:
    # "Explicit modules is enabled but the compiler was not recognized"
    # particularly in CI or when a compiler launcher/wrapper is involved.
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_EXPLICIT_MODULES "NO")
    set(CMAKE_XCODE_ATTRIBUTE_SWIFT_ENABLE_EXPLICIT_MODULES "NO")
endif()

# Enables OBS frontend API usage for UI-facing functionality.
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)

# Enables Qt-based functionality.
option(ENABLE_QT "Use Qt functionality" ON)



# ------------------------------------------------------------------------------
# Shared project configuration
# ------------------------------------------------------------------------------

include(compilerconfig)
include(defaults)
include(helpers)

include(FetchContent)

# ------------------------------------------------------------------------------
# Third-party dependencies
# ------------------------------------------------------------------------------

# JSON
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

# LibSodium via CMake wrapper, so CI/build machines do not need a preinstalled
# system package.
FetchContent_Declare(
    libsodium
    GIT_REPOSITORY https://github.com/robinlinden/libsodium-cmake.git
    GIT_TAG cfebfd3da486d5a86c644c8b47067e5411c7599c
)

# Avoid building dependency tests/examples in CI.
set(SODIUM_DISABLE_TESTS ON CACHE BOOL "Disable libsodium tests" FORCE)

FetchContent_MakeAvailable(nlohmann_json libsodium)

# ------------------------------------------------------------------------------
# Libsodium warning policy adjustments
# ------------------------------------------------------------------------------

if(TARGET sodium)
    # MSVC
    if(MSVC)
        target_compile_options(sodium PRIVATE
            /WX-
            /wd4244
            /wd4319
            /wd4267
        )
    endif()

    # GCC (Linux)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(sodium PRIVATE
            -Wno-error
            -Wno-error=type-limits
            -Wno-error=unused-parameter
            -Wno-error=unused-variable
            -Wno-unused-parameter
            -Wno-unused-variable
        )
    endif()

    # Clang / AppleClang
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
        target_compile_options(sodium PRIVATE
            -Wno-error
            -Wno-unused-parameter
            -Wno-unused-variable
            -Wno-unreachable-code
        )
    endif()

    # Xcode target-level warning-as-error override
    if(CMAKE_GENERATOR STREQUAL "Xcode")
        set_property(TARGET sodium PROPERTY XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS NO)
    endif()
endif()



# ------------------------------------------------------------------------------
# Target definition
# ------------------------------------------------------------------------------

# Build the OBS plugin as a module.
add_library(${CMAKE_PROJECT_NAME} MODULE)

# ------------------------------------------------------------------------------
# ASAN support for the plugin itself, which is useful for detecting memory issues in
# ------------------------------------------------------------------------------
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

if(ENABLE_ASAN AND MSVC)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /fsanitize=address /Zi)
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE /fsanitize=address /INCREMENTAL:NO)
endif()

# ------------------------------------------------------------------------------
# Core OBS dependency
# ------------------------------------------------------------------------------

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# ------------------------------------------------------------------------------
# Crypto dependency
# ------------------------------------------------------------------------------

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    sodium
)

# ------------------------------------------------------------------------------
# OpenSSL dependency //Deprecated in favor of libsodium
# ------------------------------------------------------------------------------

#find_package(OpenSSL REQUIRED)

#target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
#    OpenSSL::Crypto
#)

#target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
#    ${OPENSSL_INCLUDE_DIR}
#)

# ------------------------------------------------------------------------------
# JSON dependency
# ------------------------------------------------------------------------------

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
)

# Local third-party include path.
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    third_party/json
)

# ------------------------------------------------------------------------------
# Optional OBS frontend API
# ------------------------------------------------------------------------------

if(ENABLE_FRONTEND_API)
    find_package(obs-frontend-api REQUIRED)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

# ------------------------------------------------------------------------------
# Optional Qt support
# ------------------------------------------------------------------------------

if(ENABLE_QT)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Core Network)

    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
    )

    # Suppress specific Clang warnings encountered in Qt/framework builds.
    target_compile_options(
        ${CMAKE_PROJECT_NAME}
        PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
    )

    # Enable Qt auto-processing for MOC, UIC, and RCC.
    set_target_properties(
        ${CMAKE_PROJECT_NAME}
        PROPERTIES
            AUTOMOC ON
            AUTOUIC ON
            AUTORCC ON
    )
endif()

# ------------------------------------------------------------------------------
# Networking dependency
# ------------------------------------------------------------------------------

find_package(CURL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl)

# ------------------------------------------------------------------------------
# Source files
# ------------------------------------------------------------------------------

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    src/plugin-main.c
    src/taffylabs_settings.cpp
    src/taffylabs_ui.cpp
    src/taffylabs_mirror.cpp
    src/taffy_json_http.cpp
    src/lore-api.cpp
    src/lore-auth.cpp
    src/lore-deviceInfo.cpp
    src/crypto_string.cpp
    src/obs_stream_keyframe_enforcer.cpp
)

# ------------------------------------------------------------------------------
# Plugin output configuration
# ------------------------------------------------------------------------------

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})