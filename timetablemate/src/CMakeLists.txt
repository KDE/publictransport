
set( timetablemate_SRCS
   timetablemate.cpp
   main.cpp
   changelogwidget.cpp
   timetablemateview.cpp
   publictransportpreview.cpp
   accessorinfoxmlwriter.cpp
   javascriptcompletionmodel.cpp
   javascriptmodel.cpp
   javascriptparser.cpp
   ${CMAKE_CURRENT_BINARY_DIR}/javascriptcompletiongeneric.cpp

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
    ${KDE4_KDEWEBKIT_LIBS} ${KDE4_THREADWEAVER_LIBS} ${QT_QTSCRIPT_LIBRARY}
    publictransporthelper
)

install( TARGETS timetablemate ${INSTALL_TARGETS_DEFAULT_ARGS} )

# Generate completion data class from scripting.h using TimetableMateCompletionGenerator.
# Rerun when the source file (scripting.h) or the tool (TimetableMateCompletionGenerator) changes.
# Put the generated class into the binary directory.
add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/javascriptcompletiongeneric.h
                           ${CMAKE_CURRENT_BINARY_DIR}/javascriptcompletiongeneric.cpp
    COMMAND ${CMAKE_BINARY_DIR}/timetablemate/timetablematecompletiongenerator/timetablematecompletiongenerator ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/scripting.h
            ${CMAKE_BINARY_DIR}/timetablemate/timetablematecompletiongenerator/timetablematecompletiongenerator )

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
