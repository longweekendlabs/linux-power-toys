cmake_minimum_required(VERSION 3.24)
project(LinuxPowerToys LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets DBus)
find_package(KF6StatusNotifierItem 6.14 REQUIRED)
find_package(KF6WindowSystem 6.14 REQUIRED)
include(GNUInstallDirs)

set(LPT_HELPERS "${CMAKE_INSTALL_FULL_LIBEXECDIR}/linux-power-toys")
set(LPT_POLICY "${CMAKE_INSTALL_FULL_DATADIR}/polkit-1/actions/org.longweekendlabs.LinuxPowerToys.policy")
set(LPT_GRANTS "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/linux-power-toys")
configure_file(packaging/org.longweekendlabs.LinuxPowerToys.policy.in
    org.longweekendlabs.LinuxPowerToys.policy @ONLY)

configure_file(helpers/permissions.py.in permissions.py @ONLY)

add_executable(linux-power-toys src/main.cpp src/controls.cpp src/controls.h)
target_compile_definitions(linux-power-toys PRIVATE
    LPT_HELPERS="${LPT_HELPERS}" LPT_POLICY="${LPT_POLICY}" LPT_GRANTS="${LPT_GRANTS}")
file(GLOB awake_icons CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/awake/*.png")
qt_add_resources(linux-power-toys awake_artwork
    PREFIX /awake
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/awake"
    FILES ${awake_icons} assets/awake/NOTICE)
target_link_libraries(linux-power-toys PRIVATE
    Qt6::Widgets Qt6::DBus KF6::StatusNotifierItem KF6::WindowSystem)
install(TARGETS linux-power-toys)
install(FILES org.longweekendlabs.LinuxPowerToys.autostart.desktop
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart
    RENAME org.longweekendlabs.LinuxPowerToys.desktop)
install(FILES org.longweekendlabs.LinuxPowerToys.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(PROGRAMS helpers/access helpers/power helpers/touchpad
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/linux-power-toys)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/permissions.py DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/linux-power-toys)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.longweekendlabs.LinuxPowerToys.policy
    DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/lib/linux-power-toys)
