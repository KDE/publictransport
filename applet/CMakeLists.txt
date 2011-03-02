project( plasma-publictransport )

# Find the required Libaries
find_package( KDE4 REQUIRED )
include( KDE4Defaults )

add_definitions( ${QT_DEFINITIONS} ${KDE4_DEFINITIONS} )
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES} )

# We add our source code here
set( publictransport_SRCS
    publictransport.cpp
    titlewidget.cpp
    journeysearchsuggestionwidget.cpp
    departureprocessor.cpp
    departuremodel.cpp
    journeysearchparser.cpp
    journeysearchlineedit.cpp
    overlaywidget.cpp
    global.cpp
    filter.cpp
    htmldelegate.cpp
    departureinfo.cpp
    settings.cpp
    filterwidget.cpp
    datasourcetester.cpp
	timetablewidget.cpp
	routegraphicsitem.cpp )

kde4_add_ui_files( publictransport_SRCS
    publicTransportConfig.ui
    publicTransportConfigAdvanced.ui
    publicTransportAppearanceConfig.ui
    publicTransportFilterConfig.ui
    alarmConfig.ui )

# Now make sure all files get to the right place
kde4_add_plugin( plasma_applet_publictransport ${publictransport_SRCS} )

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
