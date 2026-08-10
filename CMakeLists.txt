cmake_minimum_required(VERSION 3.20)
project(shine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "LLVM ${LLVM_PACKAGE_VERSION}")
include_directories(${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(LLVM_LIBS core support target
    x86asmparser x86codegen x86desc x86info asmprinter)

# LLVM_DEFINITIONS can come back as one malformed string on some
# LLVM/CMake combos (each "-Dfoo" glued into a single token instead of a
# list), which breaks add_definitions(). Split it defensively instead.
separate_arguments(LLVM_DEFINITIONS_LIST UNIX_COMMAND "${LLVM_DEFINITIONS}")

add_executable(shinec
    src/error.cpp
    src/lexer.cpp
    src/parser.cpp
    src/codegen.cpp
    src/main.cpp
)
target_include_directories(shinec PRIVATE include)
target_link_libraries(shinec PRIVATE ${LLVM_LIBS})
target_compile_definitions(shinec PRIVATE ${LLVM_DEFINITIONS_LIST})

enable_testing()
add_subdirectory(tests)