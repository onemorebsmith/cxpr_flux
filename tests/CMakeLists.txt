cmake_minimum_required(VERSION 3.14)

project(cxpr_flux_tests)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
)
set(gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll" FORCE)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()


file(GLOB_RECURSE SOURCES "*.cpp")
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE  ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE gtest gtest_main cxpr cxpr_flux)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})