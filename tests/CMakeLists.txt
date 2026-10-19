# Catch2 integration for unit tests
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

add_subdirectory(catch2)

add_executable(unit_tests
    unit_tests.cpp
)

target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(unit_tests PRIVATE
    iconv
    charset
    Catch2::Catch2WithMain
)

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
