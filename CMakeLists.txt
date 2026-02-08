cmake_minimum_required(VERSION 3.16)
project(budget_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(budget_core
    src/core/date.cpp
    src/core/holidays.cpp
    src/core/budget.cpp
)

target_include_directories(budget_core PUBLIC src)

target_compile_features(budget_core PUBLIC cxx_std_20)

add_library(budget_io
    src/io/paths.cpp
    src/io/json_map.cpp
    src/io/json_kv.cpp
    src/io/cache.cpp
    src/io/header_map.cpp
    src/io/csv_reader.cpp
    src/io/history_finder.cpp
    src/io/history_reader.cpp
)

target_include_directories(budget_io PUBLIC src)
find_path(NLOHMANN_JSON_INCLUDE_DIR
    nlohmann/json.hpp
    PATHS ${CMAKE_SOURCE_DIR}/third_party
)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    message(FATAL_ERROR "nlohmann/json.hpp not found. Please add it under third_party/nlohmann/json.hpp or install the library.")
endif()

target_include_directories(budget_io PUBLIC ${NLOHMANN_JSON_INCLUDE_DIR})
target_link_libraries(budget_io PUBLIC budget_core)

add_library(budget_ui
    src/ui/color.cpp
    src/ui/color_utils.cpp
    src/ui/table.cpp
    src/ui/theme_config.cpp
    src/ui/dotchart.cpp
)

target_include_directories(budget_ui PUBLIC src)
target_link_libraries(budget_ui PUBLIC budget_core budget_io)

add_library(budget_cli_support
    src/cli/help.cpp
    src/cli/completion.cpp
)
target_include_directories(budget_cli_support PUBLIC src)

add_executable(core_tests tests/core_tests.cpp)

target_link_libraries(core_tests PRIVATE budget_io)

add_executable(budget src/cli/main.cpp)

find_package(Readline QUIET)
if(Readline_FOUND)
    target_link_libraries(budget PRIVATE budget_io budget_ui budget_cli_support Readline::Readline)
    target_compile_definitions(budget PRIVATE HAVE_READLINE=1)
else()
    find_library(EDITLINE_LIBRARY edit)
    find_path(EDITLINE_INCLUDE_DIR editline/readline.h)
    if(EDITLINE_LIBRARY AND EDITLINE_INCLUDE_DIR)
        target_link_libraries(budget PRIVATE budget_io budget_ui budget_cli_support ${EDITLINE_LIBRARY})
        target_include_directories(budget PRIVATE ${EDITLINE_INCLUDE_DIR})
        target_compile_definitions(budget PRIVATE HAVE_EDITLINE=1)
    else()
        target_link_libraries(budget PRIVATE budget_io budget_ui budget_cli_support)
    endif()
endif()

add_executable(completion_tests tests/completion_tests.cpp)
target_link_libraries(completion_tests PRIVATE budget_cli_support)

find_package(GTest QUIET)
if(GTest_FOUND)
    add_executable(holiday_tests tests/holiday_tests.cpp)
    target_link_libraries(holiday_tests PRIVATE budget_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(holiday_tests)
endif()
