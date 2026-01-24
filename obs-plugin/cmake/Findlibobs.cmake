# Fallback finder for libobs when a config package is not present (e.g., Windows choco install).

set(_OBS_INC_HINTS
    "$ENV{ProgramW6432}/obs-studio/include"
    "$ENV{ProgramFiles}/obs-studio/include"
)
if(DEFINED ENV{ProgramFiles\(x86\)})
    list(APPEND _OBS_INC_HINTS "$ENV{ProgramFiles\(x86\)}/obs-studio/include")
endif()

find_path(LIBOBS_INCLUDE_DIR
    NAMES obs.h
    PATHS ${_OBS_INC_HINTS}
)

set(_OBS_LIB_HINTS
    "$ENV{ProgramW6432}/obs-studio/bin/64bit"
    "$ENV{ProgramW6432}/obs-studio/obs-plugins/64bit"
    "$ENV{ProgramFiles}/obs-studio/bin/64bit"
    "$ENV{ProgramFiles}/obs-studio/obs-plugins/64bit"
)
if(DEFINED ENV{ProgramFiles\(x86\)})
    list(APPEND _OBS_LIB_HINTS
        "$ENV{ProgramFiles\(x86\)}/obs-studio/bin/64bit"
        "$ENV{ProgramFiles\(x86\)}/obs-studio/obs-plugins/64bit"
    )
endif()

find_library(LIBOBS_LIBRARY
    NAMES obs libobs
    PATHS ${_OBS_LIB_HINTS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libobs
    REQUIRED_VARS LIBOBS_INCLUDE_DIR LIBOBS_LIBRARY)

if(libobs_FOUND)
    add_library(OBS::libobs UNKNOWN IMPORTED)
    set_target_properties(OBS::libobs PROPERTIES
        IMPORTED_LOCATION "${LIBOBS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBOBS_INCLUDE_DIR}"
    )
endif()
