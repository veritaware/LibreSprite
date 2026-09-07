# Besprited | Copyright (C) 2026 Veritaware
#
# Unit test suite. Enabled with -DENABLE_TESTS=on at configure time.
#
# Every *_tests.cpp file below becomes its own standalone test executable and
# CTest entry. Discovery is automatic per module directory - drop a new
# <name>_tests.cpp into the matching test/<module>/ directory and reconfigure.

######################################################################
# GoogleTest

include(FetchContent)

# Prefer a system-provided GTest; otherwise fetch a pinned copy at configure
# time (no submodule, no network access needed once populated in the build dir).
set(BESPRITED_GTEST_VERSION 1.17.0)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v${BESPRITED_GTEST_VERSION}
  GIT_SHALLOW    TRUE
  FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)

if(NOT TARGET GTest::gtest_main)
  message(FATAL_ERROR "GoogleTest could not be provided for ENABLE_TESTS")
endif()

######################################################################
# Test discovery helper

# find_tests(<module-subdir> <link libraries...>)
#
# Globs test/<module-subdir>/*_tests.cpp and turns each into an executable
# linked against GTest::gtest_main plus the given libraries.
function(find_tests dir)
  set(deps ${ARGN})
  file(GLOB testsources CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*_tests.cpp")

  # Tests linked with "she" get its platform main(); tests.h then renames the
  # test's own entry point to app_main() so she can call it.
  list(FIND deps she _linked_with_she)

  # app-lib relies on static initializers (the FileFormat / command injection
  # registry) that a normal static link would drop - pull the whole archive in.
  set(prelibs "")
  if("app-lib" IN_LIST deps)
    list(REMOVE_ITEM deps app-lib)
    set(prelibs "$<LINK_LIBRARY:WHOLE_ARCHIVE,app-lib>")
  endif()

  # The module libraries have interdependencies that are not all expressed as
  # transitive link deps, so let the linker rescan the whole set.
  if(deps)
    set(deps "$<LINK_GROUP:RESCAN,${deps}>")
  endif()

  foreach(testsourcefile ${testsources})
    get_filename_component(testname ${testsourcefile} NAME_WE)

    add_executable(${testname} ${testsourcefile})
    add_test(NAME ${testname} COMMAND ${testname})

    target_include_directories(${testname} PRIVATE
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/third_party
      ${CMAKE_SOURCE_DIR}/third_party/observable
      ${BASE_INCLUDE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR})

    if(NOT _linked_with_she EQUAL -1)
      target_compile_definitions(${testname} PRIVATE LINKED_WITH_SHE)
    endif()

    target_link_libraries(${testname} PRIVATE
      GTest::gtest_main ${prelibs} ${deps} ${PLATFORM_LIBS})

    if(MSVC)
      # Fix problem compiling gen from a Visual Studio solution
      set_target_properties(${testname} PROPERTIES
        LINK_FLAGS -ENTRY:"mainCRTStartup")
    endif()
  endforeach()
endfunction()

######################################################################
# Module test suites

# app-lib has no transitive link dependencies of its own (the besprited target
# lists them explicitly), so app tests need the full module set spelled out.
set(APP_TEST_LIBS
  app-lib
  evalmath
  base-lib
  cfg-lib
  clip
  css-lib
  doc-lib
  filters-lib
  fixmath-lib
  flic-lib
  gfx-lib
  net-lib
  render-lib
  script-lib
  she
  ui-lib
  undo
  ${LibArchive_LIBRARY}
  ${TINYXML2_LIBRARY}
  ${JPEG_LIBRARIES}
  ${GIF_LIBRARIES}
  ${PNG_LIBRARIES}
  ${WEBP_LIBRARIES}
  ${ZLIB_LIBRARIES}
  ${FREETYPE_LIBRARIES})

find_tests(base   base-lib)
find_tests(gfx    gfx-lib)
find_tests(doc    doc-lib)
find_tests(render render-lib)
find_tests(css    css-lib)
# ui-lib calls back into app (app::i18n), so ui tests need the full set too.
find_tests(ui     ${APP_TEST_LIBS})
find_tests(app/file ${APP_TEST_LIBS})
find_tests(app    ${APP_TEST_LIBS})
