add_library(memsim_test_harness INTERFACE)
target_include_directories(memsim_test_harness INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

function(memsim_add_test src)
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE memsim_test_harness memsim_flags ${ARGN})
  if(MEMSIM_VALGRIND)
    add_test(NAME ${name} COMMAND valgrind --quiet --error-exitcode=1 --leak-check=full
             --errors-for-leak-kinds=definite,indirect $<TARGET_FILE:${name}>)
  else()
    add_test(NAME ${name} COMMAND ${name})
  endif()
endfunction()

# Unit tests (owners): tests/unit/<module>/test_*.c may use that module's private headers.
file(GLOB MEMSIM_UNIT_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/unit/*)
foreach(dir IN LISTS MEMSIM_UNIT_DIRS)
  if(NOT IS_DIRECTORY ${dir})
    continue()
  endif()
  get_filename_component(module ${dir} NAME)
  file(GLOB tests CONFIGURE_DEPENDS ${dir}/test_*.c)
  foreach(src IN LISTS tests)
    memsim_add_test(${src} memsim::${module})
    get_filename_component(name ${src} NAME_WE)
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src/${module})
  endforeach()
endforeach()

# Golden, property, oracle and integration tests (P8): exported headers only.
file(GLOB_RECURSE MEMSIM_SYSTEM_TESTS CONFIGURE_DEPENDS
  golden/test_*.c property/test_*.c oracle/test_*.c integration/test_*.c)
foreach(src IN LISTS MEMSIM_SYSTEM_TESTS)
  memsim_add_test(${src} memsim::bench)
endforeach()
