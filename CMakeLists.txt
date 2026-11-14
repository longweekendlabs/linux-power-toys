cmake_minimum_required(VERSION 3.24)
project(LinuxPowerToys LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets DBus)
find_package(KF6StatusNotifierItem 6.14 REQUIRED)
find_package(KF6WindowSystem 6.14 REQUIRED)
include(GNUInstallDirs)

add_executable(linux-power-toys src/main.cpp)
target_link_libraries(linux-power-toys PRIVATE
    Qt6::Widgets Qt6::DBus KF6::StatusNotifierItem KF6::WindowSystem)
install(TARGETS linux-power-toys)
install(FILES org.longweekendlabs.LinuxPowerToys.autostart.desktop
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart
    RENAME org.longweekendlabs.LinuxPowerToys.desktop)
install(FILES org.longweekendlabs.LinuxPowerToys.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
