project( timetablemate )

find_package( KDE4 REQUIRED )
include( KDE4Defaults )

include_directories(
	${CMAKE_SOURCE_DIR}
	${CMAKE_BINARY_DIR}
	${KDE4_INCLUDES} ${QT_INCLUDES} )

# add_subdirectory( doc )
add_subdirectory( src )
add_subdirectory( icons )
add_subdirectory( templates )
