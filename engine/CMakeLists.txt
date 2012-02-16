project(plasma-engine-publictransport)

# Find the required Libaries
find_package(KDE4 REQUIRED)
include(KDE4Defaults)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES} )

add_subdirectory( accessorInfos )

if ( BUILD_TESTS )
    add_subdirectory( tests )
endif ( BUILD_TESTS )

# Source files
set( publictransport_engine_SRCS
    publictransportdataengine.cpp
    departureinfo.cpp
    accessorinfoxmlreader.cpp
    timetableaccessor.cpp
    timetableaccessor_info.cpp
    timetableaccessor_script.cpp
    script_thread.cpp
    scripting.cpp
    enums.cpp )

# Now make sure all files get to the right place
kde4_add_plugin( plasma_engine_publictransport ${publictransport_engine_SRCS} )

target_link_libraries( plasma_engine_publictransport
    ${KDE4_PLASMA_LIBS} ${KDE4_KROSSCORE_LIBS} ${KDE4_KIO_LIBS} ${KDE4_THREADWEAVER_LIBS} )

install( TARGETS plasma_engine_publictransport
         DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES plasma-engine-publictransport.desktop
         DESTINATION ${SERVICES_INSTALL_DIR} )
