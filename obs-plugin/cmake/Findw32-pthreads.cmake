# Fallback finder for w32-pthreads built within OBS dependencies/build tree.

include(FindPackageHandleStandardArgs)

set(_W32PTHREADS_INC_HINTS
    "$ENV{OBS_SOURCE_DIR}/deps/w32-pthreads"
    "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads"
    "C:/obs-src/deps/w32-pthreads"
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

find_package_handle_standard_args(w32-pthreads REQUIRED_VARS W32PTHREADS_INCLUDE_DIR W32PTHREADS_LIBRARY)

if(w32-pthreads_FOUND)
    add_library(w32-pthreads::w32-pthreads SHARED IMPORTED)
    set_target_properties(w32-pthreads::w32-pthreads PROPERTIES
        IMPORTED_LOCATION "${W32PTHREADS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${W32PTHREADS_INCLUDE_DIR}"
    )
endif()
