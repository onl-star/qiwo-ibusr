# Author: Marguerite Su <i@marguerite.su>
# License: GPL
# Description: find Rime schema collection package.
# RIME_DATA_FOUND - System has rime-data package
# RIME_DATA_DIR - rime-data absolute path

set(RIME_DATA_FIND_DIR "${CMAKE_INSTALL_PREFIX}/share/rime-data"
                       "${CMAKE_INSTALL_PREFIX}/share/rime/data"
                       "${CMAKE_INSTALL_PREFIX}/share/brise"
                       "/usr/local/share/rime-data"
                       "/usr/local/share/rime/data"
                       "/usr/local/share/brise"
                       "/usr/share/rime-data"
                       "/usr/share/rime/data"
                       "/usr/share/brise")

find_program(PKG_CONFIG_EXECUTABLE pkg-config)
if(PKG_CONFIG_EXECUTABLE)
    foreach(_RIME_PKG_CONFIG_VAR pkgdatadir datadir rime_data_dir)
        execute_process(
            COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=${_RIME_PKG_CONFIG_VAR} rime
            OUTPUT_VARIABLE _RIME_PKG_CONFIG_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_RIME_PKG_CONFIG_DIR)
            list(APPEND RIME_DATA_FIND_DIR "${_RIME_PKG_CONFIG_DIR}")
        endif()
    endforeach()
endif()

set(RIME_DATA_FOUND FALSE)

foreach(_RIME_DATA_DIR ${RIME_DATA_FIND_DIR})
    if (IS_DIRECTORY ${_RIME_DATA_DIR})
        set(RIME_DATA_FOUND True)
        set(RIME_DATA_DIR ${_RIME_DATA_DIR})
    endif (IS_DIRECTORY ${_RIME_DATA_DIR})
endforeach(_RIME_DATA_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RimeData DEFAULT_MSG RIME_DATA_DIR)
mark_as_advanced(RIME_DATA_DIR)
