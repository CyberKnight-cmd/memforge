add_executable(memsim main.c sha256.c)
target_link_libraries(memsim PRIVATE memsim::bench memsim_flags)

# The commit goes into every trace header (section 11); "unknown" outside a git checkout.
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_VARIABLE MEMSIM_GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(MEMSIM_GIT_SHA)
  target_compile_definitions(memsim PRIVATE MEMSIM_GIT="${MEMSIM_GIT_SHA}")
endif()
