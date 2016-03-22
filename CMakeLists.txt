project( plasma-publictransport )

# Find the required Libaries
cmake_minimum_required(VERSION 2.8.12)
find_package(ECM 1.7.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${ECM_MODULE_PATH} ${ECM_MODULE_DIR} ${CMAKE_MODULE_PATH})
find_package(Qt5 5.3.0  REQUIRED COMPONENTS Core DBus Xml Quick Test)
find_package(KF5 5.19.0 REQUIRED COMPONENTS CoreAddons Config I18n NewStuff KDELibs4Support Plasma PlasmaQuick)
find_package(Marble QUIET CONFIG)
set_package_properties(Marble PROPERTIES DESCRIPTION "Marble is a virtual globe and world atlas" URL "http://marble.kde.org" TYPE OPTIONAL PURPOSE "Marble is used to show public transport stops in a map" )

include(FeatureSummary)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

include(ECMInstallIcons)
include(ECMSetupVersion)
include(ECMMarkNonGuiExecutable)
include(ECMOptionalAddSubdirectory)

include(CheckCXXCompilerFlag)

add_definitions( ${QT_DEFINITIONS} )
include_directories( ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} )

# Can be used from the command line, eg. "cmake -DINSTALL_TIMETABLEMATE=ON .."
# to configure with timetablemate (or use "cmake -i .." for interactive options)
# Can also be set from a CMake GUI
option( INSTALL_ENGINE_OPENSTREETMAP "Note: Currently unused! Only useful if you have GPS hardware. It can then be used to get stops near your current position" OFF )
option( INSTALL_APPLET "A plasma applet showing a departure/arrival board or journeys. Departures can be filtered and alarms can be set" ON )
option( INSTALL_APPLET_FLIGHTS "A simple applet that shows flight departures. It uses a single service provider, that's also available for the other applets/runner" OFF )
option( INSTALL_APPLET_GRAPHICALTIMETABLELINE "Shows a street with vehicles moving on it, representing the timetable" OFF )
option( INSTALL_RUNNER "Can be used to quickly get departures/arrivals or journeys" ON )
option( INSTALL_ICONS "Vehicle type icons and other icons used by PublicTransport components" ON )
option( INSTALL_ALL "Install everything (except for unit tests). This overrides other options and is OFF by default" OFF )
option( BUILD_TESTS "Turn this option on to build unit tests for PublicTransport" OFF )
mark_as_advanced( BUILD_TESTS )

if ( INSTALL_ALL )
    set( INSTALL_ENGINE_OPENSTREETMAP TRUE )
    set( INSTALL_APPLET TRUE )
    set( INSTALL_APPLET_FLIGHTS TRUE )
    set( INSTALL_APPLET_GRAPHICALTIMETABLELINE TRUE )
    set( INSTALL_RUNNER TRUE )
    set( INSTALL_ICONS TRUE )
endif ( INSTALL_ALL )

message( "******************************************************" )
message( "**** The following components are enabled:       ****" )
message( "******************************************************" )

# The library and the engine are used by all applets, the runner and TimetableMate
message( " - PublicTransport Helper Library" )
add_subdirectory( libpublictransporthelper )

# Include directory of publictransporthelper, to be used by other components using the library
set( PUBLICTRANSPORTHELPER_INCLUDES
    ${CMAKE_SOURCE_DIR}/libpublictransporthelper/
    ${CMAKE_BINARY_DIR}/libpublictransporthelper/ # for lib_config.h
)
if ( MARBLE_FOUND )
    list ( APPEND PUBLICTRANSPORTHELPER_INCLUDES ${MARBLE_INCLUDE_DIR} )
endif ( MARBLE_FOUND )

#[[message( " - PublicTransport Data Engine" )
add_subdirectory( engine )

if ( INSTALL_ENGINE_OPENSTREETMAP )
    message( " - OpenStreetMap Data Engine" )
    add_subdirectory( engine-openstreetmap )
endif ( INSTALL_ENGINE_OPENSTREETMAP )

# Icons are are also needed everywhere, but may be turned off
if ( INSTALL_ICONS )
    message( " - PublicTransport Icons" )
    add_subdirectory( icons )
endif ( INSTALL_ICONS )

if ( INSTALL_APPLET )
    message( " - PublicTransport Applet" )
    add_subdirectory( applet )
endif ( INSTALL_APPLET )

if ( INSTALL_APPLET_FLIGHTS )
    message( " - Flights Applet" )
    add_subdirectory( applet-flights )
endif ( INSTALL_APPLET_FLIGHTS )

if ( INSTALL_APPLET_GRAPHICALTIMETABLELINE )
    message( " - GraphicalTimetableLine Applet" )
    add_subdirectory( applet-graphicaltimetableline)
endif ( INSTALL_APPLET_GRAPHICALTIMETABLELINE )

if ( INSTALL_RUNNER )
    message( " - PublicTransport Runner" )
    add_subdirectory( runner )
endif ( INSTALL_RUNNER )]]

message( "*****************************************************" )

# Show which external packages were found
#macro_display_feature_log()
