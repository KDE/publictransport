
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

# Find protocol buffers
find_package( ProtocolBuffers REQUIRED )


if ( BUILD_TESTS )
    add_subdirectory( tests )
endif ( BUILD_TESTS )

if ( INSTALL_TIMETABLEMATE )
    message( " - TimetableMate" )
    add_subdirectory( timetablemate )
endif ( INSTALL_TIMETABLEMATE )

add_subdirectory( serviceProviders )

# Generate gtfs-realtime protocol buffer sources
WRAP_PROTO( PROTO_SRC gtfs-realtime.proto )

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
    publictransportservice.cpp
    global.cpp
)

# Add provider types (adding their sources to publictransport_engine_SRCS)
add_subdirectory( script )
add_subdirectory( gtfs )

# Now make sure all files get to the right place
kde4_add_plugin( plasma_engine_publictransport ${publictransport_engine_SRCS} ${PROTO_SRC} )

target_link_libraries( plasma_engine_publictransport
    ${KDE4_PLASMA_LIBS} ${KDE4_KIO_LIBS} ${KDE4_THREADWEAVER_LIBS} ${QT_QTSCRIPT_LIBRARY}
    ${QT_QTXML_LIBRARY} ${QT_QTSQL_LIBRARY}
    pthread protobuf ) # pthread is needed for protobuf

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
