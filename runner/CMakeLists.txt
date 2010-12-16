project(PublictransportRunner)

# Find the required Libaries
find_package(KDE4 REQUIRED)
include(KDE4Defaults)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   )

##########################
# The runner #############
##########################
set(krunner_publictransport_SRCS
    publictransportrunner.cpp
    global.cpp )
kde4_add_plugin(krunner_publictransport ${krunner_publictransport_SRCS})
target_link_libraries(krunner_publictransport
                      ${KDE4_PLASMA_LIBS} ${KDE4_KDEUI_LIBS})

##########################
# The runner config ######
##########################
set(kcm_krunner_publictransport_SRCS
    global.cpp
    config/publictransportrunner_config.cpp
    config/htmldelegate.cpp
    config/stopsettingsdialog.cpp
    config/locationmodel.cpp
    config/serviceprovidermodel.cpp )
kde4_add_ui_files( kcm_krunner_publictransport_SRCS
    config/publicTransportRunnerConfig.ui
    config/publicTransportStopConfig.ui
    config/accessorInfo.ui )
kde4_add_plugin(kcm_krunner_publictransport ${kcm_krunner_publictransport_SRCS})
target_link_libraries(kcm_krunner_publictransport
    ${KDE4_KDECORE_LIBS}
    ${KDE4_KDEUI_LIBS}
    ${KDE4_KCMUTILS_LIBS}
    ${KDE4_PLASMA_LIBS} 
    ${QT_QTCORE_LIBRARY}
    ${QT_QTGUI_LIBRARY}
)

##########################
# Install both ###########
##########################
install(TARGETS krunner_publictransport kcm_krunner_publictransport
	DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-runner-publictransport.desktop config/plasma-runner-publictransport_config.desktop
        DESTINATION ${SERVICES_INSTALL_DIR})

