project( plasma-applet-publictransport )

# Find the required Libaries
find_package( KDE4 REQUIRED )
include( KDE4Defaults )
include( ../PublicTransportDefaults )
add_definitions( ${QT_DEFINITIONS} ${KDE4_DEFINITIONS} )

include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   ${PUBLICTRANSPORTHELPER_INCLUDES}
)

# We add our source code here
set( publictransport_SRCS
    publictransport.cpp
    publictransport_p.cpp
    titlewidget.cpp
    popupicon.cpp
    departureprocessor.cpp
    departuremodel.cpp
    departurepainter.cpp
    journeysearchsuggestionwidget.cpp
    journeysearchparser.cpp
    journeysearchlineedit.cpp
    journeysearchitem.cpp
    journeysearchmodel.cpp
    journeysearchlistview.cpp
    overlaywidget.cpp
    global.cpp
    settings.cpp
    settingsio.cpp
    settingsui.cpp
    datasourcetester.cpp
    timetablewidget.cpp
    routegraphicsitem.cpp
    stopaction.cpp
    colorgroups.cpp )

kde4_add_ui_files( publictransport_SRCS
    publicTransportConfig.ui
    publicTransportConfigAdvanced.ui
    publicTransportAppearanceConfig.ui
    publicTransportFilterConfig.ui
    alarmConfig.ui )

# Now make sure all files get to the right place
kde4_add_plugin( plasma_applet_publictransport ${publictransport_SRCS} )

add_dependencies( plasma_applet_publictransport publictransporthelper )

target_link_libraries( plasma_applet_publictransport
    ${KDE4_PLASMA_LIBS}
    ${KDE4_KDEUI_LIBS}
    ${KDE4_KIO_LIBS}
    ${KDE4_KNEWSTUFF3_LIBS}
    publictransporthelper # found in ../publictransporthelper
)

install( TARGETS plasma_applet_publictransport
         DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES plasma-applet-publictransport.desktop
         DESTINATION ${SERVICES_INSTALL_DIR} )

install( FILES vehicles.svg
         DESTINATION ${DATA_INSTALL_DIR}/plasma_applet_publictransport )

if ( BUILD_TESTS )
    add_subdirectory( tests )
endif ( BUILD_TESTS )
