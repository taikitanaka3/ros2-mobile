cmake_minimum_required(VERSION 3.16)

project(android_sensor_viz)

find_package(ament_cmake REQUIRED)
find_package(pluginlib REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rviz_common REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} SHARED
  src/android_sensor_panel.cpp
  src/compass_widget.cpp
  src/android_dashboard_panel.cpp
  src/sensor_pulse_widget.cpp
  src/imu_motion_widget.cpp
  src/gnss_position_widget.cpp
  src/magnetometer_widget.cpp
  src/joystick_widget.cpp
  include/android_sensor_viz/android_sensor_panel.hpp
  include/android_sensor_viz/compass_widget.hpp
  include/android_sensor_viz/android_dashboard_panel.hpp
  include/android_sensor_viz/sensor_pulse_widget.hpp
  include/android_sensor_viz/imu_motion_widget.hpp
  include/android_sensor_viz/gnss_position_widget.hpp
  include/android_sensor_viz/magnetometer_widget.hpp
  include/android_sensor_viz/joystick_widget.hpp
)

target_include_directories(${PROJECT_NAME}
  PRIVATE
    include
)

ament_target_dependencies(${PROJECT_NAME}
  pluginlib
  builtin_interfaces
  rclcpp
  rviz_common
  sensor_msgs
  std_msgs
)

target_link_libraries(${PROJECT_NAME} Qt${QT_VERSION_MAJOR}::Widgets)

pluginlib_export_plugin_description_file(rviz_common plugin_description.xml)

install(
  TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  LIBRARY DESTINATION lib
)

install(
  DIRECTORY include/
  DESTINATION include
)

install(
  FILES plugin_description.xml
  DESTINATION share/${PROJECT_NAME}
)

install(
  DIRECTORY config/
  DESTINATION share/${PROJECT_NAME}/config
)

ament_package()
