add_executable(quantlab_tests analytics_tests.cpp)
target_link_libraries(quantlab_tests PRIVATE QuantLab::quantlab doctest::doctest)
# Third-party framework macros may use compiler extensions; keep strict warnings
# on our own code without applying them to doctest's implementation.
target_include_directories(quantlab_tests SYSTEM PRIVATE "${doctest_SOURCE_DIR}")
quantlab_configure_target(quantlab_tests)
add_test(NAME analytics_unit_tests COMMAND quantlab_tests --no-colors)
set_tests_properties(analytics_unit_tests PROPERTIES TIMEOUT 30)
add_test(NAME demo_smoke COMMAND quantlab_demo)
add_test(NAME demo_help COMMAND quantlab_demo --help)
