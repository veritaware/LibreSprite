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

  # base/log.cpp calls she::log(), which lives in the same translation unit as
  # she's platform main(). So any test that links both base and she pulls in
  # she's main() - such tests must define app_main() instead (via tests.h with
  # -DLINKED_WITH_SHE) and let she's main() call it, and must NOT also link
  # GTest::gtest_main.
  list(FIND deps she _linked_with_she)
  if(_linked_with_she EQUAL -1)
    set(entrypoint GTest::gtest_main)
  else()
    set(entrypoint GTest::gtest)
  endif()

  # Tests that link app-lib may reach for its gen-generated headers (e.g.
  # pref.xml.h, pulled in by app/pref/preferences.h) - remember that before
  # the WHOLE_ARCHIVE rewrite below removes "app-lib" from deps.
  if("app-lib" IN_LIST deps)
    set(_needs_app_lib TRUE)
  else()
    set(_needs_app_lib FALSE)
  endif()

  # app-lib and script-lib both rely on static initializers (the FileFormat /
  # command injection registry, and the Engine / InternalScriptObject / qjs
  # EngineDelegate registrations respectively) that a normal static link
  # would drop - a test that never calls a function from e.g.
  # quickjs/engine.cpp.o gives the linker no reason to pull that object file
  # out of the archive at all, silently dropping its registrations. Pull the
  # whole archive in for any such lib this suite links against.
  set(prelibs "")
  foreach(wholeArchiveLib app-lib script-lib)
    if("${wholeArchiveLib}" IN_LIST deps)
      list(REMOVE_ITEM deps ${wholeArchiveLib})
      list(APPEND prelibs "$<LINK_LIBRARY:WHOLE_ARCHIVE,${wholeArchiveLib}>")
    endif()
  endforeach()

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

    if(_needs_app_lib)
      target_include_directories(${testname} PRIVATE ${APP_INCLUDE_DIR})
      # app-lib is only reached through the WHOLE_ARCHIVE generator
      # expression in ${prelibs} below, which alone doesn't force ninja to
      # finish app-lib's own custom-command-generated headers before this
      # target's sources (which may #include one of them) start compiling.
      add_dependencies(${testname} app-lib)
    endif()

    target_link_libraries(${testname} PRIVATE
      ${entrypoint} ${prelibs} ${deps} ${PLATFORM_LIBS})

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
# lists them explicitly), and doc-lib / render-lib / ui-lib all pull in "she"
# (whose main() then owns the entry point), so every test suite that touches
# that layer links the full module set.
set(MODULE_TEST_LIBS
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

# base / gfx / css / script do not touch the she layer - keep these lean and
# let GTest::gtest_main own the entry point. script-lib pulls in qjs (and
# base-lib for base/convert_to.cpp & friends) transitively.
find_tests(base   base-lib)
find_tests(gfx    gfx-lib)
find_tests(css    css-lib)
find_tests(script script-lib base-lib)

# doc / render / ui / app all reach the she layer (see MODULE_TEST_LIBS).
find_tests(doc      ${MODULE_TEST_LIBS})
find_tests(render   ${MODULE_TEST_LIBS})
find_tests(ui       ${MODULE_TEST_LIBS})
find_tests(app/file ${MODULE_TEST_LIBS})
find_tests(app      ${MODULE_TEST_LIBS})
