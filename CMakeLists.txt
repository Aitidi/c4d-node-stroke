cmake_minimum_required(VERSION 3.20)
project(NodeStrokeCore LANGUAGES CXX)
add_library(wire_geometry src/gesture.cpp src/graph.cpp src/view_tracking.cpp src/native_view.cpp)
target_include_directories(wire_geometry PUBLIC src)
target_compile_features(wire_geometry PUBLIC cxx_std_17)
enable_testing()
add_executable(gesture_tests tests/gesture_tests.cpp)
target_link_libraries(gesture_tests PRIVATE wire_geometry)
add_test(NAME gesture_tests COMMAND gesture_tests)

add_executable(graph_tests tests/graph_tests.cpp)
target_link_libraries(graph_tests PRIVATE wire_geometry)
add_test(NAME graph_tests COMMAND graph_tests)

add_executable(view_tracking_tests tests/view_tracking_tests.cpp)
target_link_libraries(view_tracking_tests PRIVATE wire_geometry)
add_test(NAME view_tracking_tests COMMAND view_tracking_tests)


add_executable(native_view_tests tests/native_view_tests.cpp)
target_link_libraries(native_view_tests PRIVATE wire_geometry)
add_test(NAME native_view_tests COMMAND native_view_tests)

add_executable(view_binding_tests tests/view_binding_tests.cpp)
target_compile_features(view_binding_tests PRIVATE cxx_std_17)
add_test(NAME view_binding_tests COMMAND view_binding_tests)

add_executable(group_draw_data_tests tests/group_draw_data_tests.cpp)
target_compile_features(group_draw_data_tests PRIVATE cxx_std_17)
add_test(NAME group_draw_data_tests COMMAND group_draw_data_tests)

if(WIN32)
  add_library(group_capture_probe SHARED experiments/group_capture_probe.cpp)
  target_compile_features(group_capture_probe PRIVATE cxx_std_17)
  target_link_libraries(group_capture_probe PRIVATE bcrypt)
endif()

