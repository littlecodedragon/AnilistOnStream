# Fallback finder for w32-pthreads built within OBS dependencies/build tree.

include(FindPackageHandleStandardArgs)

set(_W32PTHREADS_INC_HINTS
    "$ENV{OBS_SOURCE_DIR}/deps/w32-pthreads"
    "$ENV{OBS_SOURCE_DIR}/deps/w32-pthreads/include"
    "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads"
    "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/include"
    "C:/obs-src/deps/w32-pthreads"
    "C:/obs-src/deps/w32-pthreads/include"
    "C:/obs-src/build/deps/w32-pthreads"
    "C:/obs-src/build/deps/w32-pthreads/include"
    "C:/obs-deps/include"
)

set(_W32PTHREADS_LIB_HINTS
    "$ENV{OBS_BUILD_DIR}/build/deps/w32-pthreads/Release"
    "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Release"
    "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads"
    "C:/obs-src/build/deps/w32-pthreads/Release"
    "C:/obs-src/build/deps/w32-pthreads"
    "C:/obs-deps/lib"
)

find_path(W32PTHREADS_INCLUDE_DIR NAMES pthread.h PATHS ${_W32PTHREADS_INC_HINTS})
find_library(W32PTHREADS_LIBRARY NAMES w32-pthreads pthreads pthread PATHS ${_W32PTHREADS_LIB_HINTS})
find_file(W32PTHREADS_DLL NAMES w32-pthreads.dll pthreads.dll PATHS ${_W32PTHREADS_LIB_HINTS})

find_package_handle_standard_args(w32-pthreads REQUIRED_VARS W32PTHREADS_INCLUDE_DIR W32PTHREADS_LIBRARY)

if(w32-pthreads_FOUND)
    # Create imported target expected by libobs config
    if(NOT TARGET OBS::w32-pthreads)
        add_library(OBS::w32-pthreads SHARED IMPORTED)
        if(W32PTHREADS_DLL)
            set_target_properties(OBS::w32-pthreads PROPERTIES IMPORTED_LOCATION "${W32PTHREADS_DLL}")
        endif()
        set_target_properties(OBS::w32-pthreads PROPERTIES
            IMPORTED_IMPLIB "${W32PTHREADS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${W32PTHREADS_INCLUDE_DIR}"
        )
    endif()
    # Also provide a secondary alias for convenience
    if(NOT TARGET w32-pthreads::w32-pthreads)
        add_library(w32-pthreads::w32-pthreads SHARED IMPORTED)
        if(W32PTHREADS_DLL)
            set_target_properties(w32-pthreads::w32-pthreads PROPERTIES IMPORTED_LOCATION "${W32PTHREADS_DLL}")
        endif()
        set_target_properties(w32-pthreads::w32-pthreads PROPERTIES
            IMPORTED_IMPLIB "${W32PTHREADS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${W32PTHREADS_INCLUDE_DIR}"
        )
    endif()
endif()
