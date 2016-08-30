project( publictransporthelper )

# Find the required Libaries
add_definitions( ${QT_DEFINITIONS} )

# Create lib_config.h from lib_config.h.in, ie.
# create #define's for each enabled variable from above
configure_file( lib_config.h.in lib_config.h )

set( include_dirs
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR} # For "lib_config.h"
)
if ( MARBLE_FOUND )
    list ( APPEND include_dirs ${MARBLE_INCLUDE_DIR} )
endif ( MARBLE_FOUND )

include_directories( ${include_dirs} )

set( publictransporthelper_LIB_SRCS
	locationmodel.cpp
	serviceprovidermodel.cpp
	vehicletypemodel.cpp
	dynamicwidget.cpp
	categorycombobox.cpp
	checkcombobox.cpp
	stopsettings.cpp
	stopwidget.cpp
    serviceproviderdatadialog.cpp
	stopsettingsdialog.cpp
	stoplineedit.cpp
	global.cpp
	columnresizer.cpp
	filter.cpp
	filterwidget.cpp
	departureinfo.cpp
	marbleprocess.cpp
)
if ( MARBLE_FOUND )
    list ( APPEND publictransporthelper_LIB_SRCS
           publictransportmapwidget.cpp
           popuphandler.cpp
           publictransportlayer.cpp )
endif ( MARBLE_FOUND )

set( publictransporthelper_LIB_HDRS
	publictransporthelper_export.h
	${CMAKE_CURRENT_BINARY_DIR}/lib_config.h

	# Models
	locationmodel.h
	serviceprovidermodel.h
	vehicletypemodel.h

	# Widgets
	dynamicwidget.h
	categorycombobox.h
	checkcombobox.h
	stopwidget.h
	stoplineedit.h
	filterwidget.h

	# Dialogs
	serviceproviderdatadialog.h
	stopsettingsdialog.h

	# Others
	global.h
	enums.h
	stopsettings.h
	filter.h
	departureinfo.h
	marbleprocess.h
)

if ( MARBLE_FOUND )
    list ( APPEND publictransporthelper_LIB_HDRS
           publictransportmapwidget.h
           popuphandler.h
           publictransportlayer.h )
endif ( MARBLE_FOUND )

ki18n_wrap_ui( publictransporthelper_LIB_SRCS
    providerData.ui
    stopConfig.ui )

add_library( publictransporthelper SHARED ${publictransporthelper_LIB_SRCS} )

if ( BUILD_TESTS )
    add_subdirectory( tests )
endif ( BUILD_TESTS )

set( libraries
    publictransporthelper
    KF5::Plasma
    KF5::KIOCore
    KF5::NewStuff
    KF5::KDELibs4Support
)
if ( MARBLE_FOUND )
    message( "   - Marble found, used to show stops in a map" )
    list ( APPEND libraries ${MARBLE_LIBRARIES} )
endif ( MARBLE_FOUND )
target_link_libraries( ${libraries} )

set_target_properties( publictransporthelper
	PROPERTIES VERSION 0 SOVERSION 11
)

install( TARGETS publictransporthelper
         ${INSTALL_TARGETS_DEFAULT_ARGS} )

install( FILES ${publictransporthelper_LIB_HDRS}
         DESTINATION ${INCLUDE_INSTALL_DIR}/publictransporthelper
         COMPONENT Devel )

install( FILES publictransporthelper.desktop
		 DESTINATION ${SERVICES_INSTALL_DIR} )
