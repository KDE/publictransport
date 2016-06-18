
project( plasma-engine-publictransport )

# Find the required Libaries
add_definitions( ${QT_DEFINITIONS} )
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${CMAKE_CURRENT_SOURCE_DIR}
   ${CMAKE_CURRENT_BINARY_DIR} # For "config.h"
)

# Include cmake_dependent_option command
include( CMakeDependentOption )

# Add custom CMake modules
set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules )

# Find mime type relevant stuff
set( SHARED_MIME_INFO_MINIMUM_VERSION "0.30" )
find_package( SharedMimeInfo )

# Create some variables to control the build,
# each provider type can be disabled, ie. not build, but at least one needs to be enabled
option( BUILD_PROVIDER_TYPE_SCRIPT "Build with support for scripted providers" ON )
option( BUILD_PROVIDER_TYPE_GTFS "Build with support for GTFS providers" ON )
option( ENABLE_DEBUG_ENGINE_JOBS "Enable debug output for started/finished jobs in the engine" OFF )
option( ENABLE_DEBUG_NETWORK "Enable debug output for network events in scripts" OFF )
option( ENABLE_DEBUG_SCRIPT_HELPER "Enable debug output for warnings/errors in the script 'helper' object, eg. when debugging scripts that use the 'helper' object" OFF )
option( ENABLE_DEBUG_SCRIPT_ERROR "Enable debug output for error messages from scripts" OFF )

# Fatal error if no provider types are enabled
if ( NOT BUILD_PROVIDER_TYPE_SCRIPT AND NOT BUILD_PROVIDER_TYPE_GTFS )
    message( FATAL_ERROR "   - No provider types enabled, this would make the engine useless!\n"
                         "Solution: Use at least one of the available provider types\n"
                         "          BUILD_PROVIDER_TYPE_SCRIPT or BUILD_PROVIDER_TYPE_GTFS." )
endif ( NOT BUILD_PROVIDER_TYPE_SCRIPT AND NOT BUILD_PROVIDER_TYPE_GTFS )

# Source files
qt5_wrap_cpp( publictransport_engine_MOC_SRCS enums.h )
set( publictransport_engine_SRCS
    publictransportdataengine.cpp
    datasource.cpp
    timetableservice.cpp
    global.cpp
    departureinfo.cpp
    request.cpp
    serviceprovider.cpp
    serviceproviderdata.cpp
    serviceproviderdatareader.cpp
    serviceproviderglobal.cpp
    serviceprovidertestdata.cpp
    ${publictransport_engine_MOC_SRCS}
)

if ( BUILD_PROVIDER_TYPE_GTFS )
    # Find ProtocolBuffers, needed for GTFS-realtime
    find_package( ProtocolBuffers QUIET CONFIG )
    set_package_properties( ProtocolBuffers PROPERTIES DESCRIPTION "GTFS-realtime support"
        URL "http://developers.google.com/protocol-buffers" TYPE OPTIONAL PURPOSE "Protocol buffers are used for GTFS-realtime support." )

    if ( PROTOBUF_FOUND )
        # Generate gtfs-realtime protocol buffer sources when found
        WRAP_PROTO( PROTO_SRC gtfs-realtime.proto )
        list ( APPEND publictransport_engine_SRCS ${PROTO_SRC} )
    endif ( PROTOBUF_FOUND )

    # Enable/disable GTFS-realtime support with ProtocolBuffers
    set( BUILD_GTFS_REALTIME ${PROTOBUF_FOUND} )
else ( BUILD_PROVIDER_TYPE_GTFS )
    set( BUILD_GTFS_REALTIME FALSE )
endif ( BUILD_PROVIDER_TYPE_GTFS )

# Create config.h from config.h.in, ie. create #define's for each enabled variable from above
configure_file( config.h.in config.h )

# Add option to build TimetableMate or not
option( INSTALL_TIMETABLEMATE "A tool/little IDE to add support for new service providers. Offers code completion for scripts and helps when checking the correct behaviour of the script" OFF )

# Add a variable for the service provider plugin installation directory
set( SERVICE_PROVIDER_DIR ${DATA_INSTALL_DIR}/plasma_engine_publictransport/serviceProviders )

# Add TimetableMate
if ( INSTALL_TIMETABLEMATE )
    message( "   - TimetableMate" )
    add_subdirectory( timetablemate )
endif ( INSTALL_TIMETABLEMATE )

# Add provider types (adding their sources to publictransport_engine_SRCS)
if ( BUILD_PROVIDER_TYPE_SCRIPT )
    message( "   - Build script provider type" )
    add_subdirectory( script )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

if ( BUILD_PROVIDER_TYPE_GTFS )
    message( "   - Build GTFS provider type" )
    add_subdirectory( gtfs )
endif ( BUILD_PROVIDER_TYPE_GTFS )

# Message, when debug output is enabled for specific events
if ( ENABLE_DEBUG_NETWORK )
    message( "   - Enable debug output for script network events" )
endif ( ENABLE_DEBUG_NETWORK )
if ( ENABLE_DEBUG_SCRIPT_HELPER )
    message( "   - Enable debug output for warnings/errors in the script 'helper' object" )
endif ( ENABLE_DEBUG_SCRIPT_HELPER )

# Add unit tests
if ( BUILD_TESTS )
    add_subdirectory( tests )
endif ( BUILD_TESTS )

# Now make sure all files get to the right place
add_library(plasma_engine_publictransport MODULE ${publictransport_engine_SRCS} )

# Collect all needed libraries in LIBS
set( LIBS KF5::Archive KF5::Plasma KF5::KIOCore KF5::KDELibs4Support KF5::ThreadWeaver z )

# Add libraries needed for the script provider type
if ( BUILD_PROVIDER_TYPE_SCRIPT )
    set( LIBS ${LIBS} Qt5::Core Qt5::Script )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

# Add libraries needed for the GTFS provider type
if ( BUILD_PROVIDER_TYPE_GTFS )
    set( LIBS ${LIBS} Qt5::Core Qt5::Xml Qt5::Sql )
    if ( BUILD_GTFS_REALTIME )
        set( LIBS ${LIBS} ${PROTOBUF_LIBRARY} pthread ) # pthread is needed for protobuf
    endif ( BUILD_GTFS_REALTIME )
endif ( BUILD_PROVIDER_TYPE_GTFS )

# Link all collected libraries
target_link_libraries( plasma_engine_publictransport ${LIBS} )

# Install engine and it's .desktop-file
install( TARGETS plasma_engine_publictransport
         DESTINATION ${PLUGIN_INSTALL_DIR} )
install( FILES plasma-engine-publictransport.desktop
         DESTINATION ${SERVICES_INSTALL_DIR} )

# Install mimetype "application/x-publictransport-engine",
# files with this mimetype are the XML-files used for service provider plugins for the
# PublicTransport data engine.
# In TimetableMate they are also used as project files, without additional data
install( FILES serviceproviderplugin.xml DESTINATION ${XDG_MIME_INSTALL_DIR} )
update_xdg_mimetypes( ${XDG_MIME_INSTALL_DIR} )

# Install .operations-file describing the "Timetable" service
# for timetable data sources
install( FILES timetable.operations
         DESTINATION ${DATA_INSTALL_DIR}/plasma/services )
