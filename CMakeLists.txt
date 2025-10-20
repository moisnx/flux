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
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
    endif()

    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_compile_options(/utf-8)
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# ============================================
# Find ncurses/ncursesw
# ============================================
# Use PROJECT_SOURCE_DIR instead of CMAKE_SOURCE_DIR for proper subproject support
set(DEPS_DIR ${PROJECT_SOURCE_DIR}/deps)

if(WIN32)
    # Windows: Use PDCursesMod (already has UTF-8 support via VT mode)
    set(PDCURSESMOD_DIR ${DEPS_DIR}/PDCursesMod/wincon)

    if(EXISTS "${PDCURSESMOD_DIR}/pdcurses.lib")
        message(STATUS "Using locally built PDCursesMod from ${PDCURSESMOD_DIR}")

        add_library(pdcursesmod STATIC IMPORTED)
        set_target_properties(pdcursesmod PROPERTIES
            IMPORTED_LOCATION "${PDCURSESMOD_DIR}/pdcurses.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${DEPS_DIR}/PDCursesMod"
        )

        set(CURSES_LIBRARIES pdcursesmod)
        set(CURSES_INCLUDE_DIRS "${DEPS_DIR}/PDCursesMod")
        message(STATUS "PDCursesMod supports UTF-8 via VT mode")
    else()
        message(FATAL_ERROR "PDCursesMod not found at ${PDCURSESMOD_DIR}. Please build it manually with nmake -f Makefile.vc HAVE_VT=Y")
    endif()

elseif(ANDROID)
    # Android: Try ncursesw first, fall back to ncurses
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NCURSESW ncursesw)

    if(NCURSESW_FOUND)
        message(STATUS "Found ncursesw for Android")
        set(CURSES_LIBRARIES ${NCURSESW_LIBRARIES})
        set(CURSES_INCLUDE_DIRS ${NCURSESW_INCLUDE_DIRS})
        set(CURSES_LINK_LIBRARIES ${NCURSESW_LINK_LIBRARIES})
    else()
        message(STATUS "ncursesw not found, falling back to ncurses")
        pkg_check_modules(NCURSES REQUIRED ncurses)
        set(CURSES_LIBRARIES ${NCURSES_LIBRARIES})
        set(CURSES_INCLUDE_DIRS ${NCURSES_INCLUDE_DIRS})
        set(CURSES_LINK_LIBRARIES ${NCURSES_LINK_LIBRARIES})
    endif()

else()
    # Linux/macOS: Prefer ncursesw over ncurses
    find_package(PkgConfig)

    if(PkgConfig_FOUND)
        # Try pkg-config first for ncursesw
        pkg_check_modules(NCURSESW ncursesw)

        if(NCURSESW_FOUND)
            message(STATUS "Found ncursesw via pkg-config")
            set(CURSES_LIBRARIES ${NCURSESW_LIBRARIES})
            set(CURSES_INCLUDE_DIRS ${NCURSESW_INCLUDE_DIRS})
            set(CURSES_LINK_FLAGS ${NCURSESW_LDFLAGS})
        endif()
    endif()

    # If pkg-config didn't work, try FindCurses with wide character preference
    if(NOT NCURSESW_FOUND)
        set(CURSES_NEED_WIDE TRUE)
        find_package(Curses REQUIRED)

        # Verify we got wide character support
        if(CURSES_HAVE_NCURSESW_NCURSES_H OR CURSES_HAVE_NCURSESW_H)
            message(STATUS "Found ncursesw via FindCurses")
        elseif(CURSES_HAVE_NCURSES_NCURSES_H OR CURSES_HAVE_NCURSES_H)
            message(WARNING "Only found ncurses (not ncursesw). Unicode icons may not render correctly.")
            message(WARNING "Install ncursesw: apt-get install libncursesw5-dev (Debian/Ubuntu)")
        else()
            message(STATUS "Found curses library: ${CURSES_LIBRARIES}")
        endif()

        set(CURSES_LIBRARIES ${CURSES_LIBRARIES})
        set(CURSES_INCLUDE_DIRS ${CURSES_INCLUDE_DIR})
    endif()
endif()



# ============================================
# Flux Library (Core + UI)
# ============================================

set(FLUX_CORE_SOURCES
    src/core/browser.cpp
    # src/core/file_ops.cpp
    # src/core/selection.cpp
    # src/core/search.cpp
)

set(FLUX_UI_SOURCES
    src/ui/renderer.cpp
    src/ui/theme.cpp
    src/ui/icon_provider.cpp
    # src/ui/preview.cpp
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
    ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(flux
    PUBLIC
    ${CURSES_LIBRARIES}
)

# Windows-specific: link winmm for beep()
if(WIN32)
    target_link_libraries(flux PUBLIC winmm)
endif()

# Android-specific
if(ANDROID AND CURSES_LINK_LIBRARIES)
    target_link_libraries(flux PUBLIC ${CURSES_LINK_LIBRARIES})
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
# Standalone Binary (fx)
# ============================================

if(BUILD_STANDALONE)
    set(STANDALONE_SOURCES
        fx/main.cpp
        fx/src/theme_loader.cpp
        fx/src/file_opener.cpp
    )

    add_executable(fx ${STANDALONE_SOURCES})

    # Link to the main library 'flux' and curses
    target_link_libraries(fx PRIVATE flux ${CURSES_LIBRARIES})

    target_include_directories(fx PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/fx
        ${CURSES_INCLUDE_DIRS}
    )

    # Windows-specific settings for PDCursesMod
    if(WIN32)
        # CRITICAL: These must match how PDCursesMod was built
        target_compile_definitions(fx PRIVATE
            PDC_STATIC # Static linking
            PDC_WIDE # Wide character support
            PDC_FORCE_UTF8 # UTF-8 support
        )

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
# Examples
# ============================================

# if(BUILD_EXAMPLES)
#     # Simple embedding example
#     add_executable(example_simple examples/embed_simple.cpp)
#     target_link_libraries(example_simple PRIVATE flux)
#     target_include_directories(example_simple PRIVATE ${PROJECT_SOURCE_DIR}/include)

#     # Custom theme example
#     add_executable(example_theme examples/embed_custom_theme.cpp)
#     target_link_libraries(example_theme PRIVATE flux)
#     target_include_directories(example_theme PRIVATE ${PROJECT_SOURCE_DIR}/include)

#     # File picker example
#     add_executable(example_picker examples/file_picker.cpp)
#     target_link_libraries(example_picker PRIVATE flux)
#     target_include_directories(example_picker PRIVATE ${PROJECT_SOURCE_DIR}/include)
# endif()

# ============================================
# Installation (Only if enabled and standalone build)
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

    # FIXED: Use PROJECT_SOURCE_DIR instead of CMAKE_SOURCE_DIR
    # This ensures it works both standalone and as a subdirectory
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
message(STATUS "Curses library: ${CURSES_LIBRARIES}")
if(NCURSESW_FOUND OR CURSES_HAVE_NCURSESW_H OR CURSES_HAVE_NCURSESW_NCURSES_H)
    message(STATUS "Wide character support: YES (ncursesw)")
else()
    message(STATUS "Wide character support: UNKNOWN (verify with ldd)")
endif()
message(STATUS "")
message(STATUS "Build options:")
message(STATUS " 	Standalone binary (fx): ${BUILD_STANDALONE}")
message(STATUS " 	Examples: ${BUILD_EXAMPLES}")
message(STATUS " 	Shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")
message(STATUS "")
