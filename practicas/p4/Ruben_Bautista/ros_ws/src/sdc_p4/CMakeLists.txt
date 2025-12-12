cmake_minimum_required(VERSION 3.8)
project(sdc_p4)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# Find ROS2 packages
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# Include directories
include_directories(include)

# Create executable with single source file
add_executable(bully
  src/bully.cpp
)

# Link libraries
target_link_libraries(bully
  ${rclcpp_LIBRARIES}
)

# Add dependencies
ament_target_dependencies(bully
  rclcpp
  std_msgs
)

# Install executable - IMPORTANT: Name must be 'bully' as specified
install(TARGETS bully
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

# Install include directory
install(DIRECTORY include/
  DESTINATION include/
)

# Export dependencies
ament_export_dependencies(rclcpp std_msgs)
ament_export_include_directories(include)
ament_export_libraries(bully)

# Build tests if any
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

# Finalize package
ament_package()