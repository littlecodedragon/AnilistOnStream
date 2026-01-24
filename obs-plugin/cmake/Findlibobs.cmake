# Fallback finder for libobs when a config package is not present (e.g., Windows choco install).

 set(_pf64 "$ENV{ProgramW6432}")
 set(_pf   "$ENV{ProgramFiles}")
 set(_pf86 "$ENV{ProgramFiles(x86)}")
 
 set(_OBS_INC_HINTS)
 if(NOT _pf64 STREQUAL "")
     list(APPEND _OBS_INC_HINTS "${_pf64}/obs-studio/include")
 endif()
 if(NOT _pf STREQUAL "")
     list(APPEND _OBS_INC_HINTS "${_pf}/obs-studio/include")
 endif()
 if(NOT _pf86 STREQUAL "")
     list(APPEND _OBS_INC_HINTS "${_pf86}/obs-studio/include")
 endif()

find_path(LIBOBS_INCLUDE_DIR
    NAMES obs.h
    PATHS ${_OBS_INC_HINTS}
)

 set(_OBS_LIB_HINTS)
 if(NOT _pf64 STREQUAL "")
     list(APPEND _OBS_LIB_HINTS
         "${_pf64}/obs-studio/bin/64bit"
         "${_pf64}/obs-studio/obs-plugins/64bit"
     )
 endif()
 if(NOT _pf STREQUAL "")
     list(APPEND _OBS_LIB_HINTS
         "${_pf}/obs-studio/bin/64bit"
         "${_pf}/obs-studio/obs-plugins/64bit"
     )
 endif()
 if(NOT _pf86 STREQUAL "")
     list(APPEND _OBS_LIB_HINTS
         "${_pf86}/obs-studio/bin/64bit"
         "${_pf86}/obs-studio/obs-plugins/64bit"
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
