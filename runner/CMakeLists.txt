project(PublictransportRunner)

# Find the required Libaries
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include( ../PublicTransportDefaults )
add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   ${PUBLICTRANSPORTHELPER_INCLUDES}
   )

##########################
# The runner config ######
##########################
set(kcm_krunner_publictransport_SRCS
    config/publictransportrunner_config.cpp )
kde4_add_ui_files( kcm_krunner_publictransport_SRCS
    config/publicTransportRunnerConfig.ui )
kde4_add_plugin( kcm_krunner_publictransport ${kcm_krunner_publictransport_SRCS} )

add_dependencies( kcm_krunner_publictransport publictransporthelper )

target_link_libraries( kcm_krunner_publictransport
    ${KDE4_KDECORE_LIBS}
    ${KDE4_KDEUI_LIBS}
    ${KDE4_KCMUTILS_LIBS}
    ${KDE4_PLASMA_LIBS}
    ${QT_QTCORE_LIBRARY}
    ${QT_QTGUI_LIBRARY}
    publictransporthelper # in ../publictransporthelper
)
# Add the publictransporthelper library sub directory (extern)
# add_subdirectory( publictransporthelper )

##########################
# The runner #############
##########################
set(krunner_publictransport_SRCS
    publictransportrunner.cpp )
kde4_add_plugin(krunner_publictransport ${krunner_publictransport_SRCS})
target_link_libraries(krunner_publictransport
	${KDE4_PLASMA_LIBS}
	${KDE4_KDEUI_LIBS}
    publictransporthelper # in ../publictransporthelper
)

add_dependencies( krunner_publictransport kcm_krunner_publictransport )

##########################
# Install both ###########
##########################
install( TARGETS krunner_publictransport kcm_krunner_publictransport
		 DESTINATION ${PLUGIN_INSTALL_DIR})
install( FILES plasma-runner-publictransport.desktop config/plasma-runner-publictransport_config.desktop
         DESTINATION ${SERVICES_INSTALL_DIR})

