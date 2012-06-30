
project( plasma-engine-publictransport )

# Find the required Libaries
find_package( KDE4 REQUIRED )
include( KDE4Defaults )
add_definitions( ${QT_DEFINITIONS} ${KDE4_DEFINITIONS} )
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   ${CMAKE_CURRENT_SOURCE_DIR} )

# Add custom CMake modules
set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

# Find mime type relevant stuff
set( SHARED_MIME_INFO_MINIMUM_VERSION "0.30" )
find_package( SharedMimeInfo )

set( BUILD_PROVIDER_TYPE_SCRIPT ON CACHE BOOL "Build with support for scripted providers")
set( BUILD_PROVIDER_TYPE_GTFS ON CACHE BOOL "Build with support for GTFS providers")
set( BUILD_GTFS_REALTIME ON CACHE BOOL "Build with GTFS-realtime support for GTFS providers (needs protocol buffers)")
configure_file( config.h.in config.h )

# Fatal error if no provider types are enabled
if ( NOT BUILD_PROVIDER_TYPE_SCRIPT AND NOT BUILD_PROVIDER_TYPE_GTFS )
    message( FATAL_ERROR "   - No provider types enabled, this would make the engine useless!\n"
                         "Solution: Use at least one of the available provider types\n"
                         "          BUILD_PROVIDER_TYPE_SCRIPT or BUILD_PROVIDER_TYPE_GTFS." )
endif ( NOT BUILD_PROVIDER_TYPE_SCRIPT AND NOT BUILD_PROVIDER_TYPE_GTFS )

# Add TimetableMate
if ( INSTALL_TIMETABLEMATE )
    message( "   - TimetableMate" )
    add_subdirectory( timetablemate )
endif ( INSTALL_TIMETABLEMATE )

# Source files
set( publictransport_engine_SRCS
    publictransportdataengine.cpp
    departureinfo.cpp
    serviceproviderdatareader.cpp
    request.cpp
    serviceprovider.cpp
    serviceproviderdata.cpp
    serviceproviderglobal.cpp
    serviceprovidertestdata.cpp
    global.cpp
)

# Add provider types (adding their sources to publictransport_engine_SRCS)
if ( BUILD_PROVIDER_TYPE_SCRIPT )
    message( "   - Build script provider type" )
    add_subdirectory( script )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

if ( BUILD_PROVIDER_TYPE_GTFS )
    message( "   - Build GTFS provider type" )
    add_subdirectory( gtfs )

    if ( BUILD_GTFS_REALTIME )
        message( "     - Build GTFS-realtime support" )
        find_package( ProtocolBuffers REQUIRED )

        # Generate gtfs-realtime protocol buffer sources
        WRAP_PROTO( PROTO_SRC gtfs-realtime.proto )
    endif ( BUILD_GTFS_REALTIME )
endif ( BUILD_PROVIDER_TYPE_GTFS )

# Add unit tests
if ( BUILD_TESTS )
    add_subdirectory( tests )
endif ( BUILD_TESTS )

# Now make sure all files get to the right place
kde4_add_plugin( plasma_engine_publictransport ${publictransport_engine_SRCS} ${PROTO_SRC} )

# Collect all needed libraries in LIBS
set( LIBS ${KDE4_PLASMA_LIBS} ${KDE4_KIO_LIBS} ${KDE4_THREADWEAVER_LIBS} )

# Add libraries needed for the script provider type
if ( BUILD_PROVIDER_TYPE_SCRIPT )
    set( LIBS ${LIBS} ${QT_QTSCRIPT_LIBRARY} )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

# Add libraries needed for the GTFS provider type
if ( BUILD_PROVIDER_TYPE_GTFS )
    set( LIBS ${LIBS} ${QT_QTXML_LIBRARY} ${QT_QTSQL_LIBRARY} )
    if ( BUILD_GTFS_REALTIME )
        set( LIBS ${LIBS} pthread protobuf ) # pthread is needed for protobuf
    endif ( BUILD_GTFS_REALTIME )
endif ( BUILD_PROVIDER_TYPE_GTFS )

target_link_libraries( plasma_engine_publictransport ${LIBS} )

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

install( FILES publictransport.operations
         DESTINATION ${DATA_INSTALL_DIR}/plasma/services )
