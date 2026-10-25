add_executable(memsim main.c)
target_link_libraries(memsim PRIVATE memsim::bench memsim_flags)
