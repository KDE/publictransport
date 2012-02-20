# find_package( ${KDE4_WEBKIT_LIBS} )
# include_directories(${LibXML++_INCLUDE_DIRS})
# set(LIBS ${LIBS} ${LibXML++_LIBRARIES})

set( timetablemate_SRCS
   timetablemate.cpp
   main.cpp
   changelogwidget.cpp
   timetablemateview.cpp
   publictransportpreview.cpp
   accessorinfoxmlwriter.cpp
#    accessorinfoxmlreader.cpp
   javascriptcompletionmodel.cpp
   javascriptmodel.cpp
   javascriptparser.cpp

   # Use files directly from the data engine
   ../../engine/global.cpp
   ../../engine/scripting.cpp
   ../../engine/script_thread.cpp
   ../../engine/departureinfo.cpp
   ../../engine/timetableaccessor.cpp
   ../../engine/timetableaccessor_info.cpp
   ../../engine/timetableaccessor_script.cpp
   ../../engine/accessorinfoxmlreader.cpp
)

kde4_add_ui_files( timetablemate_SRCS
    timetablemateview_base.ui
    prefs_base.ui )

kde4_add_kcfg_files( timetablemate_SRCS settings.kcfgc )

kde4_add_executable( timetablemate ${timetablemate_SRCS} )

add_dependencies( timetablemate publictransporthelper )

target_link_libraries( timetablemate ${KDE4_KDEUI_LIBS} ${KDE4_KPARTS_LIBS}
    ${KDE4_PLASMA_LIBS} ${KDE4_KTEXTEDITOR_LIBS} ${KDE4_KROSSCORE_LIBS}
    ${QT_QTWEBKIT_LIBRARY} ${KDE4_KDEWEBKIT_LIBS} ${KDE4_THREADWEAVER_LIBS}
    publictransporthelper
)

install( TARGETS timetablemate ${INSTALL_TARGETS_DEFAULT_ARGS} )


########### install files ###############
install( PROGRAMS timetablemate.desktop  DESTINATION ${XDG_APPS_INSTALL_DIR} )
install( FILES timetablemate.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES timetablemateui.rc  DESTINATION  ${DATA_INSTALL_DIR}/timetablemate )

### KAuth
kde4_install_auth_actions( org.kde.timetablemate timetablemate.actions )

kde4_add_executable( timetablematehelper timetablematehelper.cpp )
target_link_libraries( timetablematehelper ${KDE4_KDECORE_LIBS} )
install( TARGETS timetablematehelper DESTINATION ${LIBEXEC_INSTALL_DIR} )
kde4_install_auth_helper_files( timetablematehelper org.kde.timetablemate root )
