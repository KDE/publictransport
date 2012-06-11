project(plasma-engine-publictransport)

# Find the required Libaries
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES} )

set( SHARED_MIME_INFO_MINIMUM_VERSION "0.30" )
find_package( SharedMimeInfo )

add_subdirectory( serviceProviders )

if ( BUILD_TESTS )
    add_subdirectory( tests )
endif ( BUILD_TESTS )

if ( INSTALL_TIMETABLEMATE )
    message( " - TimetableMate" )
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
    serviceproviderscript.cpp
    script_thread.cpp
    scripting.cpp
    global.cpp
)

# Now make sure all files get to the right place
kde4_add_plugin( plasma_engine_publictransport ${publictransport_engine_SRCS} )

target_link_libraries( plasma_engine_publictransport
    ${KDE4_PLASMA_LIBS} ${KDE4_KIO_LIBS} ${KDE4_THREADWEAVER_LIBS} ${QT_QTSCRIPT_LIBRARY}
)

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
