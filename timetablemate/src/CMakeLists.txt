# find_package( ${KDE4_WEBKIT_LIBS} )
# include_directories(${LibXML++_INCLUDE_DIRS})
# set(LIBS ${LIBS} ${LibXML++_LIBRARIES})

set( timetablemate_SRCS
   timetablemate.cpp
   main.cpp
   changelogwidget.cpp
   timetablemateview.cpp
   publictransportpreview.cpp
   accessorinfoxmlreader.cpp
   javascriptcompletionmodel.cpp
   javascriptmodel.cpp
   javascriptparser.cpp
   scripting.cpp
)

kde4_add_ui_files( timetablemate_SRCS
    timetablemateview_base.ui
    prefs_base.ui )

kde4_add_kcfg_files( timetablemate_SRCS settings.kcfgc )

kde4_add_executable( timetablemate ${timetablemate_SRCS} )

if ( KDE4_KDEWEBKIT_LIBRARY )
	target_link_libraries( timetablemate ${KDE4_KDEUI_LIBS} ${KDE4_KPARTS_LIBS} 
		${KDE4_PLASMA_LIBS} ${KDE4_KTEXTEDITOR_LIBS} ${KDE4_KROSSCORE_LIBS} ${KDE4_WEBKIT_LIBS}
		publictransporthelper
	)
else ( KDE4_KDEWEBKIT_LIBRARY )
	# KWebKit not found
	target_link_libraries( timetablemate ${KDE4_KDEUI_LIBS} ${KDE4_KPARTS_LIBS} 
		${KDE4_PLASMA_LIBS} ${KDE4_KTEXTEDITOR_LIBS} ${KDE4_KROSSCORE_LIBS}
		publictransporthelper
	)
endif ( KDE4_KDEWEBKIT_LIBRARY )

install( TARGETS timetablemate ${INSTALL_TARGETS_DEFAULT_ARGS} )


########### install files ###############
install( FILES timetablemate.desktop  DESTINATION ${XDG_APPS_INSTALL_DIR} )
install( FILES timetablemate.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES timetablemateui.rc  DESTINATION  ${DATA_INSTALL_DIR}/timetablemate )

### KAuth
kde4_install_auth_actions( org.kde.timetablemate timetablemate.actions )

kde4_add_executable( timetablematehelper timetablematehelper.cpp )
target_link_libraries( timetablematehelper ${KDE4_KDECORE_LIBS} )
install( TARGETS timetablematehelper DESTINATION ${LIBEXEC_INSTALL_DIR} )
kde4_install_auth_helper_files( timetablematehelper org.kde.timetablemate root )