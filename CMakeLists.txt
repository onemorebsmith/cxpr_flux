project(cxpr_flux VERSION "0.1" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)


option(CXPR_FLUX_BUILD_TESTS "Build and run tests" ON)

file(GLOB_RECURSE HEADERS "cxpr_flux/*.h")

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE ${HEADERS})
target_include_directories(${PROJECT_NAME} INTERFACE $<INSTALL_INTERFACE:include> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cxpr_flux>)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)

if(CXPR_FLUX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()


install(DIRECTORY cxpr_flux/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# This makes the project importable from the build directory
export(TARGETS ${PROJECT_NAME} FILE ${PROJECT_NAME}Config.cmake)
export(PACKAGE ${PROJECT_NAME})