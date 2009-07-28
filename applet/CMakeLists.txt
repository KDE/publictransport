# Project Needs a name ofcourse
project(plasma-publictransport)

# Find the required Libaries
find_package(KDE4 REQUIRED)
include(KDE4Defaults)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   )

# We add our source code here
set(publictransport_SRCS publictransport.cpp global.cpp settings.cpp datasourcetester.cpp alarmtimer.cpp htmldelegate.cpp departureinfo.cpp)

kde4_add_ui_files(publictransport_SRCS publicTransportConfig.ui publicTransportConfigAdvanced.ui publicTransportAppearanceConfig.ui publicTransportFilterConfig.ui accessorInfo.ui)

# Now make sure all files get to the right place
kde4_add_plugin(plasma_applet_publictransport ${publictransport_SRCS})

target_link_libraries( plasma_applet_publictransport
                      ${KDE4_PLASMA_LIBS} ${KDE4_KDEUI_LIBS} )

install(TARGETS plasma_applet_publictransport
        DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES plasma-applet-publictransport.desktop
        DESTINATION ${SERVICES_INSTALL_DIR})

# install(FILES public-transport-stop.svgz
#         DESTINATION ${KDE4_ICON_INSTALL_DIR})

add_subdirectory(kbflags)
