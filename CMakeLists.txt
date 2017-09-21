cmake_minimum_required(VERSION 3.5.1 FATAL_ERROR)
project(oalsfxpp_test VERSION 1.0.0 LANGUAGES CXX)

set(
    sources
    oalsfxpp.cpp
    oalsfxpp_test.cpp
)

set(
    headers
    oalsfxpp.h
)

add_executable(
    oalsfxpp_test
    ${sources}
    ${headers}
)

set_target_properties(
    oalsfxpp_test
    PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

set_target_properties(
    oalsfxpp_test
    PROPERTIES
    OUTPUT_NAME "oalsfxpp_test"
    PROJECT_LABEL "oalsfxpp test"
)

install(
    TARGETS
    oalsfxpp_test
    RUNTIME DESTINATION .
)
