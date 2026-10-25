# memsim_module(<name> SOURCES ... [DEPS ...])
#
# Defines the static library memsim_<name> for the module in the current directory:
#   include/  exported headers, visible to every target that links this module
#   *.h       private headers, visible only inside this module (and its unit tests)
# A module can only include what it links, so DEPS is the layering contract (README section 5).
function(memsim_module name)
  cmake_parse_arguments(ARG "" "" "SOURCES;DEPS" ${ARGN})
  add_library(memsim_${name} STATIC ${ARG_SOURCES})
  add_library(memsim::${name} ALIAS memsim_${name})
  target_include_directories(memsim_${name}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  list(TRANSFORM ARG_DEPS PREPEND memsim::)
  target_link_libraries(memsim_${name} PUBLIC ${ARG_DEPS} PRIVATE memsim_flags)
endfunction()
