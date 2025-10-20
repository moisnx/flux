# PDCursesMod Linking Issues on Windows - Complete Guide

## Problem Overview

When building Windows applications that link against PDCursesMod, you may encounter **unresolved external symbol** errors like:

```
error LNK2019: símbolo externo endwin_x64_4400 sin resolver
```

This happens because PDCursesMod uses **name mangling** with version suffixes (e.g., `endwin` becomes `endwin_x64_4400`), and the linker cannot find the correct symbols.

---

## Root Cause

PDCursesMod requires **exact matching** of preprocessor definitions between:

1. How the library was built
2. How your application is compiled

### Key Preprocessor Definitions

- **`PDC_STATIC`** - Indicates static linking (vs DLL)
- **`PDC_WIDE`** - Enables wide character (Unicode) support
- **`PDC_FORCE_UTF8`** - Forces UTF-8 encoding for proper international character handling
- **`PDC_DLL_BUILD`** - Should NOT be defined when linking statically

### The Name Mangling Issue

PDCursesMod mangles function names based on:

- Platform architecture (x64, x86)
- Version number (e.g., 4400)
- Build configuration

If your executable doesn't have the same preprocessor definitions as the library, the linker looks for function names that don't exist in the compiled library.

---

## The Solution: Proper CMake Configuration

### For the Library Target (`flux`)

The flux library should already have proper configuration:

```cmake
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
    endif()
endif()

# Link libraries
target_link_libraries(flux PUBLIC ${CURSES_LIBRARIES})

# Windows-specific: link winmm for beep()
if(WIN32)
    target_link_libraries(flux PUBLIC winmm)
endif()
```

### For Executable Targets (`fx`) - THE CRITICAL PART

**This is where the fix happens.** You MUST add the preprocessor definitions to any executable that links against PDCursesMod:

```cmake
if(BUILD_STANDALONE)
    set(STANDALONE_SOURCES
        fx/main.cpp
        fx/src/theme_loader.cpp
    )

    add_executable(fx ${STANDALONE_SOURCES})

    # Link to the main library 'flux' and curses
    target_link_libraries(fx PRIVATE flux ${CURSES_LIBRARIES})

    target_include_directories(fx PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/fx
        ${CURSES_INCLUDE_DIRS}
    )

    # ========================================
    # CRITICAL: Windows-specific PDCursesMod settings
    # ========================================
    if(WIN32)
        # These definitions MUST match how PDCursesMod was built
        target_compile_definitions(fx PRIVATE
            PDC_STATIC          # Static linking (not DLL)
            PDC_WIDE            # Wide character support
            PDC_FORCE_UTF8      # UTF-8 support for international characters
        )

        # Link additional Windows libraries required by PDCursesMod
        target_link_libraries(fx PRIVATE winmm)

        # If using MSVC, ensure static runtime to avoid DLL dependencies
        if(MSVC)
            target_compile_options(fx PRIVATE
                $<$<CONFIG:Debug>:/MTd>    # Multi-threaded Debug static
                $<$<CONFIG:Release>:/MT>   # Multi-threaded Release static
            )
        endif()
    endif()
endif()
```

---

## Why This Works

### 1. **Consistent Preprocessor Definitions**

By adding `PDC_STATIC`, `PDC_WIDE`, and `PDC_FORCE_UTF8` to your executable, you ensure the compiler generates function calls with the same mangled names that exist in the PDCursesMod library.

### 2. **Proper Include Order**

The definitions must be set **before** `#include <curses.h>` is processed. CMake's `target_compile_definitions()` ensures this happens automatically.

### 3. **Static Runtime Linking (MSVC)**

Using `/MT` (Release) and `/MTd` (Debug) prevents runtime DLL dependencies and ensures your executable is fully self-contained.

---

## How PDCursesMod Should Be Built

For reference, PDCursesMod should be built with these settings:

```cmd
cd deps/PDCursesMod/wincon
nmake -f Makefile.vc WIDE=Y UTF8=Y HAVE_VT=Y
```

Key flags:

- **`WIDE=Y`** - Enables wide character support
- **`UTF8=Y`** - Enables UTF-8 encoding
- **`HAVE_VT=Y`** - Enables Windows Virtual Terminal sequences for ANSI color support

This creates `pdcurses.lib` in the `deps/PDCursesMod/wincon` directory.

---

## Common Mistakes to Avoid

### ❌ DON'T: Define preprocessor macros in source code

```cpp
// BAD: This is inconsistent and won't work reliably
#ifdef _WIN32
#undef PDC_DLL_BUILD
#define PDC_STATIC
#include <curses.h>
#endif
```

### ✅ DO: Use CMake target_compile_definitions

```cmake
# GOOD: Consistent across all translation units
if(WIN32)
    target_compile_definitions(fx PRIVATE PDC_STATIC PDC_WIDE PDC_FORCE_UTF8)
endif()
```

### ❌ DON'T: Forget to link winmm

PDCursesMod's `beep()` function requires Windows Multimedia library:

```cmake
target_link_libraries(fx PRIVATE winmm)  # Required!
```

### ❌ DON'T: Mix debug and release builds

Always rebuild everything when switching between Debug and Release configurations:

```cmd
cmake --build . --config Release --clean-first
```

---

## Troubleshooting Checklist

If you still get linking errors after applying the fix:

1. **Clean and rebuild everything:**

   ```cmd
   cd build
   cmake --build . --config Release --clean-first --verbose
   ```

2. **Verify PDCursesMod exists:**

   ```cmd
   dir deps\PDCursesMod\wincon\pdcurses.lib
   ```

3. **Check for mixed architectures:**

   - Ensure both PDCursesMod and your project are built for the same architecture (x64 or x86)
   - Check CMake's generator: `cmake -G "Visual Studio 16 2019" -A x64 ..`

4. **Verify preprocessor definitions are applied:**
   Look for these in the verbose build output:

   ```
   /DPDCURSES_STATIC /DPDCURSES_WIDE /DPDCURSES_FORCE_UTF8
   ```

5. **Check symbol names with dumpbin:**
   ```cmd
   dumpbin /SYMBOLS deps\PDCursesMod\wincon\pdcurses.lib | findstr endwin
   ```
   You should see mangled names like `endwin_x64_4400`

---

## Testing the Fix

After applying the solution, build and run:

```cmd
# Clean build
cd build
del /F /S /Q *
cd ..

# Reconfigure
cmake -B build -G "Visual Studio 16 2019" -A x64

# Build
cmake --build build --config Release --verbose

# Run
build\Release\fx.exe
```

If the build succeeds and the executable runs without errors, the fix is working correctly.

---

## Additional Notes

### When to Rebuild PDCursesMod

Rebuild PDCursesMod if:

- Switching between architectures (x86 ↔ x64)
- Changing wide character support settings
- Updating to a new version of PDCursesMod
- Changing from static to shared library (or vice versa)

### For Other Executables or Tests

Apply the same pattern to ANY executable that links against PDCursesMod:

```cmake
add_executable(my_test test.cpp)
target_link_libraries(my_test PRIVATE flux ${CURSES_LIBRARIES})

if(WIN32)
    target_compile_definitions(my_test PRIVATE PDC_STATIC PDC_WIDE PDC_FORCE_UTF8)
    target_link_libraries(my_test PRIVATE winmm)
endif()
```

### Cross-Platform Compatibility

This fix is Windows-specific. On Linux/macOS, ncurses/ncursesw doesn't require these definitions:

```cmake
if(WIN32)
    # PDCursesMod-specific definitions
    target_compile_definitions(fx PRIVATE PDC_STATIC PDC_WIDE PDC_FORCE_UTF8)
    target_link_libraries(fx PRIVATE winmm)
elseif(UNIX)
    # ncurses/ncursesw works without special definitions
    # Just ensure you're linking ncursesw for Unicode support
endif()
```

---

## Summary

**The key takeaway:** When linking executables against PDCursesMod on Windows, you MUST add `PDC_STATIC`, `PDC_WIDE`, and `PDC_FORCE_UTF8` as compile definitions to the executable target, not just the library target. This ensures consistent name mangling between compilation units and resolves unresolved external symbol errors.

Save this document for future reference whenever you encounter PDCursesMod linking issues!
