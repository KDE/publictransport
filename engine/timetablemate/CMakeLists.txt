project( timetablemate )

find_package( KDE4 REQUIRED )
include( KDE4Defaults )
include( ../../PublicTransportDefaults )
include_directories(
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}
        ${KDE4_INCLUDES}
        ${PUBLICTRANSPORTHELPER_INCLUDES}
        ${CMAKE_CURRENT_BINARY_DIR}/../
         )

set( DOCUMENTATION_DIR ${DATA_INSTALL_DIR}/timetablemate/doc )

add_subdirectory( completiongenerator )
add_subdirectory( src )
add_subdirectory( doc )
add_subdirectory( icons )
add_subdirectory( svg )
add_subdirectory( templates )
