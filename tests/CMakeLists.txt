add_executable(shine_tests
    test_main.cpp
    test_lexer.cpp
    test_parser.cpp
    test_typecheck.cpp
    ${CMAKE_SOURCE_DIR}/src/error.cpp
    ${CMAKE_SOURCE_DIR}/src/lexer.cpp
    ${CMAKE_SOURCE_DIR}/src/parser.cpp
    ${CMAKE_SOURCE_DIR}/src/typecheck.cpp
)
target_include_directories(shine_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME shine_tests COMMAND shine_tests)