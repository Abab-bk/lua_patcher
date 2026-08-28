cmake_minimum_required(VERSION 3.21)

project(LuaHarness LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Linux-only smoke harness for the Lua bindings. Compiles the real plugin
# sources against mocked RE types, sol2 and the system Lua interpreter.

include(FetchContent)

# sol2 is header-only; v3.5.0 plus the same lua-5.5 compatibility patch the
# vcpkg port carries (sol2 upstream only supports up to Lua 5.4). The patch
# step is idempotent: skip when already applied (FetchContent re-runs it).
set(SOL2_LUA55_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/vcpkg/ports/sol2/lua-5.5.diff")
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.5.0
    PATCH_COMMAND
        sh -c "git apply -R --check --ignore-whitespace '${SOL2_LUA55_PATCH}' 2>/dev/null || git apply --ignore-whitespace '${SOL2_LUA55_PATCH}'")
FetchContent_MakeAvailable(sol2)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA5_5 REQUIRED IMPORTED_TARGET lua5.5)
pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)

add_executable(
    LuaHarness
    LuaHarness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/LuaApi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/LeveledList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/Equipment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/Magic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/FormList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/ScriptLoader.cpp)

target_include_directories(
    LuaHarness PRIVATE
    mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${sol2_SOURCE_DIR}/include
    ${LUA5_5_INCLUDE_DIRS})

target_link_libraries(LuaHarness PRIVATE PkgConfig::LUA5_5 PkgConfig::FMT)

# The plugin sources rely on PCH.h for the logger alias, fmt and the common
# standard headers; the real build force-includes it via
# target_precompile_headers. Mirror that here so the same TUs compile unchanged.
target_compile_options(
    LuaHarness PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/../../src/PCH.h" -Wno-unknown-pragmas -Wno-unused-parameter -Wno-comment)

# Match the plugin build: sol2's argument type checks default to OFF in release
# builds (NDEBUG); force them on so the harness exercises the same checks.
target_compile_definitions(LuaHarness PRIVATE SOL_ALL_SAFETIES_ON=1)

enable_testing()
# The harness reads the shipped example scripts via repo-root-relative paths,
# so run it from the repository root rather than the build directory.
add_test(NAME LuaHarness COMMAND LuaHarness WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")