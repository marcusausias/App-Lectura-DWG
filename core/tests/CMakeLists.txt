add_executable(dwgcore_tests test_geometry.cpp)
target_link_libraries(dwgcore_tests PRIVATE dwgcore)
add_test(NAME dwgcore_tests COMMAND dwgcore_tests)
