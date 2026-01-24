# Fallback finder for SIMDe (header-only) from OBS build/deps or obs-deps bundle.

include(FindPackageHandleStandardArgs)

set(_SIMDE_INC_HINTS
    "$ENV{OBS_SOURCE_DIR}/deps/simde"
    "$ENV{OBS_BUILD_DIR}/deps/simde"
    "C:/obs-src/deps/simde"
    "C:/obs-deps/include"
)

find_path(SIMDE_INCLUDE_DIR NAMES simde/simde.h PATHS ${_SIMDE_INC_HINTS})

find_package_handle_standard_args(SIMDe REQUIRED_VARS SIMDE_INCLUDE_DIR)

if(SIMDe_FOUND)
    add_library(SIMDe::SIMDe INTERFACE IMPORTED)
    set_target_properties(SIMDe::SIMDe PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SIMDE_INCLUDE_DIR}"
    )
endif()
