project( publictransporthelper )

# Find the required Libaries
find_package( KDE4 REQUIRED )
include( KDE4Defaults )

add_definitions( ${QT_DEFINITIONS} ${KDE4_DEFINITIONS} )

option( BUILD_MARBLE_MAP "Use the MarbleWidget to show stops in it" ON )

# Create config.h from config.h.in, ie. create #define's for each enabled variable from above
configure_file( config.h.in config.h )

set( include_dirs
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${KDE4_INCLUDES}
    ${CMAKE_CURRENT_BINARY_DIR} # For "config.h"
)
if ( BUILD_MARBLE_MAP )
    list ( APPEND include_dirs /usr/include/marble )
endif ( BUILD_MARBLE_MAP )

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
	stopfinder.cpp
	htmldelegate.cpp
	global.cpp
	columnresizer.cpp
	filter.cpp
	filterwidget.cpp
	departureinfo.cpp
)
if ( BUILD_MARBLE_MAP )
    list ( APPEND publictransporthelper_LIB_SRCS
           publictransportmapwidget.cpp
           popuphandler.cpp
           publictransportlayer.cpp )
endif ( BUILD_MARBLE_MAP )

set( publictransporthelper_LIB_HDRS
	publictransporthelper_export.h
	${CMAKE_CURRENT_BINARY_DIR}/config.h

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
	stopfinder.h
	filter.h
	departureinfo.h
)
# TODO
# if ( BUILD_MARBLE_MAP )
#     list ( APPEND publictransporthelper_LIB_SRCS
           #   publictransportmapwidget.h
           #    popuphandler.h
           #    publictransportlayer.h )
# endif ( BUILD_MARBLE_MAP )

kde4_add_ui_files( publictransporthelper_LIB_SRCS
    providerData.ui
    stopConfig.ui )

kde4_add_library( publictransporthelper SHARED ${publictransporthelper_LIB_SRCS} )

if ( BUILD_TESTS )
    add_subdirectory( tests )
endif ( BUILD_TESTS )

set( libraries
    publictransporthelper
	${KDE4_PLASMA_LIBS}
	${KDE4_KDEUI_LIBS}
	${KDE4_KIO_LIBS}
	${KDE4_KNEWSTUFF3_LIBS}
)
if ( BUILD_MARBLE_MAP )
    message( "   - Build marble map" )
    list ( APPEND libraries marblewidget )
endif ( BUILD_MARBLE_MAP )
target_link_libraries( ${libraries} )

set_target_properties( publictransporthelper
	PROPERTIES VERSION 0 SOVERSION 10
)

install( TARGETS publictransporthelper
         ${INSTALL_TARGETS_DEFAULT_ARGS} )

install( FILES ${publictransporthelper_LIB_HDRS}
         DESTINATION ${INCLUDE_INSTALL_DIR}/publictransporthelper
         COMPONENT Devel )

install( FILES publictransport.knsrc
	     DESTINATION ${CONFIG_INSTALL_DIR} )

install( FILES publictransporthelper.desktop
		 DESTINATION ${SERVICES_INSTALL_DIR} )
