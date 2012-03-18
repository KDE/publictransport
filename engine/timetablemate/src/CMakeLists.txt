
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
   debugger.cpp
   debuggerstructures.cpp
   debuggeragent.cpp
   debuggerjobs.cpp
   ${CMAKE_CURRENT_BINARY_DIR}/javascriptcompletiongeneric.cpp

   # Use files directly from the data engine
   ../../global.cpp
   ../../scripting.cpp
   ../../script_thread.cpp
   ../../departureinfo.cpp
   ../../timetableaccessor.cpp
   ../../timetableaccessor_info.cpp
   ../../timetableaccessor_script.cpp
   ../../accessorinfoxmlreader.cpp
)

kde4_add_ui_files( timetablemate_SRCS timetablemateview_base.ui )
kde4_add_kcfg_files( timetablemate_SRCS settings.kcfgc )
kde4_add_executable( timetablemate ${timetablemate_SRCS} )

add_dependencies( timetablemate publictransporthelper )

target_link_libraries( timetablemate ${KDE4_KDEUI_LIBS}
    ${KDE4_KPARTS_LIBS} # For the different main view tabs
    ${KDE4_KTEXTEDITOR_LIBS} # For the text editor main view tabs (for XML and script files)
    ${KDE4_KDEWEBKIT_LIBS} # For the web main view tab
    ${KDE4_PLASMA_LIBS} # For the Plasma preview main view tab
    ${KDE4_THREADWEAVER_LIBS} # For includes from the engine
    ${QT_QTSCRIPT_LIBRARY}
    ${QT_QTSCRIPTTOOLS_LIBRARY} # For the debugger
    publictransporthelper
)

install( TARGETS timetablemate ${INSTALL_TARGETS_DEFAULT_ARGS} )

# Generate completion data class from scripting.h using CompletionGenerator.
# Rerun when the source file (scripting.h) or the tool (CompletionGenerator) changes.
# Put the generated class into the binary directory.
add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/javascriptcompletiongeneric.h
                           ${CMAKE_CURRENT_BINARY_DIR}/javascriptcompletiongeneric.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/../doc/index.html
                           ${CMAKE_CURRENT_SOURCE_DIR}/../doc/helper.html
                           ${CMAKE_CURRENT_SOURCE_DIR}/../doc/network.html
                           ${CMAKE_CURRENT_SOURCE_DIR}/../doc/networkrequest.html
                           ${CMAKE_CURRENT_SOURCE_DIR}/../doc/resultobject.html
                           ${CMAKE_CURRENT_SOURCE_DIR}/../doc/storage.html
                           ${CMAKE_CURRENT_SOURCE_DIR}/../doc/enums.html
                           ${CMAKE_CURRENT_SOURCE_DIR}/../doc/default.css
    COMMAND ${CMAKE_BINARY_DIR}/engine/timetablemate/completiongenerator/completiongenerator
            --verbose
            --out_completion ${CMAKE_CURRENT_BINARY_DIR}
            --out_doc ${CMAKE_CURRENT_SOURCE_DIR}/../doc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../scripting.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../../enums.h
            ${CMAKE_BINARY_DIR}/engine/timetablemate/completiongenerator/completiongenerator )

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
