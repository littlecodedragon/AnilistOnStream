# Fallback finder for libobs when a config package is not present.

include(FindPackageHandleStandardArgs)

if(WIN32)
    # Windows: locate headers, import library and optional DLL
    # Allow explicit hints via environment variables
    set(_OBS_SDK_DIR "$ENV{OBS_SDK_DIR}")
    set(_OBS_SRC_DIR "$ENV{OBS_SOURCE_DIR}")
    set(_OBS_BUILD_DIR "$ENV{OBS_BUILD_DIR}")

    # Standard Program Files locations (x64, x86)
    set(_pf64 "$ENV{ProgramW6432}")
    set(_pf   "$ENV{ProgramFiles}")
    set(_pf86 "$ENV{ProgramFiles\(x86\)}")

    # Include hints
    set(_OBS_INC_HINTS)
    if(_OBS_SDK_DIR AND NOT _OBS_SDK_DIR STREQUAL "")
        list(APPEND _OBS_INC_HINTS
            "${_OBS_SDK_DIR}/include"
            "${_OBS_SDK_DIR}/include/obs"
        )
    endif()
    if(_OBS_SRC_DIR AND NOT _OBS_SRC_DIR STREQUAL "")
        # OBS source tree headers
        list(APPEND _OBS_INC_HINTS "${_OBS_SRC_DIR}/libobs" "${_OBS_SRC_DIR}/include")
    endif()
    if(NOT _pf64 STREQUAL "")
        list(APPEND _OBS_INC_HINTS "${_pf64}/obs-studio/include")
    endif()
    if(NOT _pf STREQUAL "")
        list(APPEND _OBS_INC_HINTS "${_pf}/obs-studio/include")
    endif()
    if(NOT _pf86 STREQUAL "")
        list(APPEND _OBS_INC_HINTS "${_pf86}/obs-studio/include")
    endif()

    find_path(LIBOBS_INCLUDE_DIR NAMES obs.h PATHS ${_OBS_INC_HINTS})

    # Library (import lib) and DLL hints
    set(_OBS_LIB_HINTS)
    if(_OBS_SDK_DIR AND NOT _OBS_SDK_DIR STREQUAL "")
        list(APPEND _OBS_LIB_HINTS
            "${_OBS_SDK_DIR}/lib"
            "${_OBS_SDK_DIR}/bin/64bit"
        )
    endif()
    if(_OBS_BUILD_DIR AND NOT _OBS_BUILD_DIR STREQUAL "")
        list(APPEND _OBS_LIB_HINTS
            "${_OBS_BUILD_DIR}/build_x64/lib"
            "${_OBS_BUILD_DIR}/rundir/Release/bin/64bit"
            "${_OBS_BUILD_DIR}/build/lib"
            "${_OBS_BUILD_DIR}/build/Release"
            "${_OBS_BUILD_DIR}/build/lib/Release"
            "${_OBS_BUILD_DIR}/build/lib/RelWithDebInfo"
            "${_OBS_BUILD_DIR}/build/RelWithDebInfo"
        )
    endif()
    if(NOT _pf64 STREQUAL "")
        list(APPEND _OBS_LIB_HINTS
            "${_pf64}/obs-studio/bin/64bit"
            "${_pf64}/obs-studio/obs-plugins/64bit"
            "${_pf64}/obs-studio/lib/64bit"
        )
    endif()
    if(NOT _pf STREQUAL "")
        list(APPEND _OBS_LIB_HINTS
            "${_pf}/obs-studio/bin/64bit"
            "${_pf}/obs-studio/obs-plugins/64bit"
            "${_pf}/obs-studio/lib/64bit"
        )
    endif()
    if(NOT _pf86 STREQUAL "")
        list(APPEND _OBS_LIB_HINTS
            "${_pf86}/obs-studio/bin/64bit"
            "${_pf86}/obs-studio/obs-plugins/64bit"
            "${_pf86}/obs-studio/lib/64bit"
        )
    endif()

    find_library(LIBOBS_IMPLIB NAMES obs libobs obs.lib libobs.lib PATHS ${_OBS_LIB_HINTS})
    find_file(LIBOBS_DLL NAMES libobs.dll obs.dll PATHS ${_OBS_LIB_HINTS})

    find_package_handle_standard_args(libobs REQUIRED_VARS LIBOBS_INCLUDE_DIR LIBOBS_IMPLIB)

    if(libobs_FOUND)
        if(NOT TARGET OBS::libobs)
            add_library(OBS::libobs SHARED IMPORTED)
            if(LIBOBS_DLL)
                set_target_properties(OBS::libobs PROPERTIES IMPORTED_LOCATION "${LIBOBS_DLL}")
            endif()
            set_target_properties(OBS::libobs PROPERTIES
                IMPORTED_IMPLIB "${LIBOBS_IMPLIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBOBS_INCLUDE_DIR}"
            )
        endif()
    endif()
else()
    # Unix: try pkg-config, else search common include/lib paths
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBOBS_PC QUIET libobs)
    endif()

    if(LIBOBS_PC_FOUND)
        # Use pkg-config results
        set(LIBOBS_INCLUDE_DIR "${LIBOBS_PC_INCLUDE_DIRS}")
        find_library(LIBOBS_LIBRARY NAMES obs libobs PATHS ${LIBOBS_PC_LIBRARY_DIRS} /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)
    else()
        # Fallback paths
        find_path(LIBOBS_INCLUDE_DIR NAMES obs.h PATHS /usr/include/obs /usr/local/include/obs /usr/include /usr/local/include)
        find_library(LIBOBS_LIBRARY NAMES obs libobs PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)
    endif()

    find_package_handle_standard_args(libobs REQUIRED_VARS LIBOBS_INCLUDE_DIR LIBOBS_LIBRARY)

    if(libobs_FOUND)
        if(NOT TARGET OBS::libobs)
            add_library(OBS::libobs SHARED IMPORTED)
            set_target_properties(OBS::libobs PROPERTIES
                IMPORTED_LOCATION "${LIBOBS_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBOBS_INCLUDE_DIR}"
            )
        endif()
    endif()
endif()
