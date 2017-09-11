project( plasma-publictransport )

# Find the required Libaries
cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

set(QT_MIN_VERSION "5.6.1")
set(KF5_MIN_VERSION "5.23.0")

find_package(ECM 1.7.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH
    ${ECM_MODULE_DIR}
    ${ECM_MODULE_PATH}
    ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

find_package(Qt5 ${QT_MIN_VERSION}  REQUIRED COMPONENTS Core DBus Xml Quick Test Sql)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS CoreAddons Config I18n NewStuff ThreadWeaver KDELibs4Support Plasma PlasmaQuick)

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
option( INSTALL_ICONS "Vehicle type icons and other icons used by PublicTransport components" OFF )
option( INSTALL_ALL "Install everything (except for unit tests). This overrides other options and is OFF by default" OFF )
option( BUILD_TESTS "Turn this option on to build unit tests for PublicTransport" OFF )
mark_as_advanced( BUILD_TESTS )

if ( INSTALL_ALL )
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

message( " - PublicTransport Data Engine" )
add_subdirectory( engine )

# Icons are are also needed everywhere, but may be turned off
if ( INSTALL_ICONS )
    message( " - PublicTransport Icons" )
    add_subdirectory( icons )
endif ( INSTALL_ICONS )


message( "*****************************************************" )

# Show which external packages were found
#macro_display_feature_log()
