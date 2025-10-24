cmake_minimum_required(VERSION 3.16)
project(flux VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
# Options
# ============================================
option(BUILD_STANDALONE "Build standalone flux binary" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_SHARED_LIBS "Build as shared library" OFF)
option(FLUX_INSTALL "Enable installation" ON)

# ============================================
# Platform-Specific Settings
# ============================================
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif()

    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_compile_options(/utf-8)
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    endif()
endif()


find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# ============================================
# Find Notcurses (Required for all builds)
# ============================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED notcurses)

if(NOT NOTCURSES_FOUND)
    if(WIN32)
        message(FATAL_ERROR
            "Notcurses library not found.\n"
            "Install via MSYS2: pacman -S mingw-w64-x86_64-notcurses\n"
            "Make sure you're building in MSYS2 MinGW64 environment."
        )
    else()
        message(FATAL_ERROR
            "Notcurses library not found.\n"
            "Install on Ubuntu/Debian: sudo apt-get install libnotcurses-dev"
        )
    endif()
endif()

message(STATUS "Found Notcurses: ${NOTCURSES_LIBRARIES}")
message(STATUS "Notcurses include dirs: ${NOTCURSES_INCLUDE_DIRS}")

# ============================================
# Flux Library (Core)
# ============================================

set(FLUX_CORE_SOURCES
    src/core/browser.cpp
    src/core/file_clipboard.cpp
)

set(FLUX_CONFIG_SOURCES
    # src/config/config.cpp
    # src/config/keybindings.cpp
)

set(FLUX_ALL_SOURCES
    ${FLUX_CORE_SOURCES}
    ${FLUX_UI_SOURCES}
    ${FLUX_CONFIG_SOURCES}
)

# Create the library
if(BUILD_SHARED_LIBS)
    add_library(flux SHARED ${FLUX_ALL_SOURCES})
else()
    add_library(flux STATIC ${FLUX_ALL_SOURCES})
endif()

# Add alias for consistent target naming
add_library(Flux::flux ALIAS flux)

# Library properties
target_include_directories(flux
    PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

# Windows-specific: link winmm for beep()
if(WIN32)
    target_link_libraries(flux PUBLIC winmm)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(flux PRIVATE /W4 /MP)
    target_compile_options(flux PRIVATE
        $<$<CONFIG:Debug>:/MTd>
        $<$<CONFIG:Release>:/MT>
    )
else()
    target_compile_options(flux PRIVATE -Wall -Wextra -pedantic)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(flux PRIVATE -g -O0 -fno-omit-frame-pointer)
    else()
        target_compile_options(flux PRIVATE -O2)
    endif()
endif()

# ============================================
# Standalone Binary (fx) - Uses Notcurses
# ============================================

if(BUILD_STANDALONE)
    set(STANDALONE_SOURCES
        fx/main.cpp
        fx/src/theme_loader.cpp
        fx/src/file_opener.cpp
        fx/src/input_prompt.cpp
        # UI
        fx/src/ui/renderer.cpp
        fx/src/ui/icon_provider.cpp
        fx/src/ui/theme.cpp
        fx/src/ui/theme_selector.cpp
        fx/src/ui/notification.cpp
    )

    add_executable(fx ${STANDALONE_SOURCES})

    # Link to the main library 'flux' and Notcurses
    target_link_libraries(fx PRIVATE
        flux
        ${NOTCURSES_LIBRARIES}
    )

    target_include_directories(fx PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/fx
        ${NOTCURSES_INCLUDE_DIRS}
    )

    # Add Notcurses library directories
    if(NOTCURSES_LIBRARY_DIRS)
        target_link_directories(fx PRIVATE ${NOTCURSES_LIBRARY_DIRS})
    endif()

    # Windows-specific settings
    if(WIN32)
        # Link additional Windows libraries
        target_link_libraries(fx PRIVATE winmm)

        # If using MSVC, ensure static runtime
        if(MSVC)
            target_compile_options(fx PRIVATE
                $<$<CONFIG:Debug>:/MTd>
                $<$<CONFIG:Release>:/MT>
            )
        endif()
    endif()

    # Install binary and data files if installation is enabled
    if(FLUX_INSTALL)
        # Install the binary
        install(TARGETS fx
            RUNTIME DESTINATION bin
        )

        # Install themes to system directory
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fx/config/themes)
            install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fx/config/themes/
                DESTINATION share/fx/themes
                FILES_MATCHING PATTERN "*.toml"
            )
        endif()

        # Install default config as example
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fx/config/config.toml)
            install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/fx/config/config.toml
                DESTINATION share/fx
                RENAME config.toml.example
            )
        endif()

        # Install documentation/README if it exists
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fx/README.md)
            install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/fx/README.md
                DESTINATION share/doc/fx
            )
        endif()

        # Post-install message
        message(STATUS "fx will be installed to: ${CMAKE_INSTALL_PREFIX}/bin")
        message(STATUS "Themes will be installed to: ${CMAKE_INSTALL_PREFIX}/share/fx/themes")
        message(STATUS "Example config will be at: ${CMAKE_INSTALL_PREFIX}/share/fx/config.toml.example")
        message(STATUS "")
        message(STATUS "After installation, run:")
        message(STATUS "  fx --init-config")
        message(STATUS "to initialize your user configuration at ~/.config/fx/")
    endif()
endif()

# ============================================
# Installation (Only if enabled)
# ============================================

if(FLUX_INSTALL)
    # Install library
    install(TARGETS flux
        EXPORT FluxTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )

    # Install headers
    install(DIRECTORY include/flux
        DESTINATION include
    )

    # Install CMake config files
    install(EXPORT FluxTargets
        FILE FluxTargets.cmake
        NAMESPACE Flux::
        DESTINATION lib/cmake/flux
    )

    # Create and install config file
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/FluxConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    if(EXISTS "${PROJECT_SOURCE_DIR}/cmake/FluxConfig.cmake.in")
        configure_package_config_file(
            "${PROJECT_SOURCE_DIR}/cmake/FluxConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/FluxConfig.cmake"
            INSTALL_DESTINATION lib/cmake/flux
        )

        install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/FluxConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/FluxConfigVersion.cmake"
            DESTINATION lib/cmake/flux
        )
    else()
        message(WARNING "FluxConfig.cmake.in not found, skipping CMake config installation")
    endif()
endif()

# ============================================
# Build Summary
# ============================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Flux File Browser Build Configuration")
message(STATUS "========================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "")
message(STATUS "UI Library: Notcurses ${NOTCURSES_VERSION}")
message(STATUS "Notcurses libraries: ${NOTCURSES_LIBRARIES}")
message(STATUS "")
message(STATUS "Build options:")
message(STATUS "        Standalone binary (fx): ${BUILD_STANDALONE}")
message(STATUS "        Examples: ${BUILD_EXAMPLES}")
message(STATUS "        Shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")
message(STATUS "")
