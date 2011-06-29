project( publictransporthelper )

# Find the required Libaries
find_package( KDE4 REQUIRED )
include( KDE4Defaults )

add_definitions( ${QT_DEFINITIONS} ${KDE4_DEFINITIONS} )
include_directories(
	${CMAKE_SOURCE_DIR}
	${CMAKE_BINARY_DIR}
	${KDE4_INCLUDES} )   

set( publictransporthelper_LIB_SRCS 
	locationmodel.cpp
	serviceprovidermodel.cpp
	vehicletypemodel.cpp
	dynamicwidget.cpp
	categorycombobox.cpp
	checkcombobox.cpp
	stopsettings.cpp
	stopwidget.cpp
	accessorinfodialog.cpp
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

kde4_add_ui_files( publictransporthelper_LIB_SRCS
	accessorInfo.ui
	stopConfig.ui )

set( publictransporthelper_LIB_HDRS
	publictransporthelper_export.h
	
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
	accessorinfodialog.h
	stopsettingsdialog.h

	# Others
	global.h
	enums.h
	stopsettings.h
	stopfinder.h
	filter.h
	departureinfo.h
# 	htmldelegate.h
)

kde4_add_library( publictransporthelper SHARED ${publictransporthelper_LIB_SRCS} )

add_subdirectory( tests )

target_link_libraries( publictransporthelper
	${KDE4_PLASMA_LIBS}
	${KDE4_KDEUI_LIBS}
	${KDE4_KIO_LIBS}
	${KDE4_KNEWSTUFF3_LIBS}
)
set_target_properties( publictransporthelper
	PROPERTIES VERSION 0 SOVERSION 10
)

install( TARGETS publictransporthelper ${INSTALL_TARGETS_DEFAULT_ARGS} )

install( FILES ${publictransporthelper_LIB_HDRS}
		 DESTINATION ${INCLUDE_INSTALL_DIR}/publictransporthelper
		 COMPONENT Devel )

install( FILES publictransport.knsrc
	     DESTINATION ${CONFIG_INSTALL_DIR} )

install( FILES publictransporthelper.desktop 
		 DESTINATION ${SERVICES_INSTALL_DIR} )
