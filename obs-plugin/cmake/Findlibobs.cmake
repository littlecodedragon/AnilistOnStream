# Fallback finder for libobs when a config package is not present (e.g., Windows choco install).

find_path(LIBOBS_INCLUDE_DIR
    NAMES obs.h
    PATHS
        "$ENV{PROGRAMFILES}/obs-studio/include"
        "$ENV{PROGRAMFILES(X86)}/obs-studio/include"
)

find_library(LIBOBS_LIBRARY
    NAMES obs libobs
    PATHS
        "$ENV{PROGRAMFILES}/obs-studio/bin/64bit"
        "$ENV{PROGRAMFILES}/obs-studio/obs-plugins/64bit"
        "$ENV{PROGRAMFILES(X86)}/obs-studio/bin/64bit"
        "$ENV{PROGRAMFILES(X86)}/obs-studio/obs-plugins/64bit"
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
