
# Mark GTFS protocol buffer files from the engine as generated
set_source_files_properties( ../../gtfs-realtime.pb.h ../../gtfs-realtime.pb.cc
                             PROPERTIES GENERATED TRUE )

qt4_wrap_cpp( timetablemate_MOC_SRCS ../../enums.h )
set( timetablemate_SRCS
   main.cpp
   timetablemate.cpp
   project.cpp
   projectmodel.cpp
   projectsettingsdialog.cpp
   changelogwidget.cpp
   serviceproviderdatawriter.cpp
   serviceproviderdatatester.cpp
   javascriptcompletionmodel.cpp
   javascriptmodel.cpp
   javascriptparser.cpp
   testmodel.cpp
   networkmonitormodel.cpp

   ${CMAKE_CURRENT_BINARY_DIR}/javascriptcompletiongeneric.cpp

   # Use files directly from the data engine
   ../../global.cpp
   ../../departureinfo.cpp
   ../../request.cpp
   ../../serviceprovider.cpp
   ../../serviceproviderdata.cpp
   ../../serviceprovidertestdata.cpp
   ../../serviceproviderglobal.cpp
   ../../serviceproviderdatareader.cpp
   ${timetablemate_MOC_SRCS}
)

if ( BUILD_PROVIDER_TYPE_SCRIPT )
    set ( timetablemate_SRCS ${timetablemate_SRCS}
        ../../script/serviceproviderscript.cpp
        ../../script/scripting.cpp
        ../../script/script_thread.cpp
    )

    add_subdirectory( debugger )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

add_subdirectory( docks )
add_subdirectory( tabs )

# Add unit tests
if ( BUILD_TESTS )
    add_subdirectory( tests )
endif ( BUILD_TESTS )

kde4_add_ui_files( timetablemate_SRCS timetablemateview_base.ui preferences.ui )
kde4_add_kcfg_files( timetablemate_SRCS settings.kcfgc )
kde4_add_executable( timetablemate ${timetablemate_SRCS} )

add_dependencies( timetablemate publictransporthelper )

# Collect all needed libraries in LIBS
set( LIBS ${KDE4_KDEUI_LIBS}
    ${KDE4_KPARTS_LIBS} # For the different main view tabs
    ${KDE4_KTEXTEDITOR_LIBS} # For the text editor main view tabs (for XML and script files)
    ${KDE4_KDEWEBKIT_LIBS} # For the web main view tab
    ${KDE4_PLASMA_LIBS} # For the Plasma preview main view tab
    ${KDE4_THREADWEAVER_LIBS} # For the debugger and for includes from the engine
    kdeclarative # For KDeclarative, no ${KDE4_KDECLARATIVE_LIBS} available..
    ${QT_QTDECLARATIVE_LIBRARY} # For the dashboard main view tabs
    publictransporthelper
    z # zlib
)

if ( BUILD_PROVIDER_TYPE_SCRIPT )
    set( LIBS ${LIBS} ${QT_QTSCRIPT_LIBRARY} )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

if ( BUILD_PROVIDER_TYPE_GTFS )
    set( LIBS ${LIBS} ${QT_QTXML_LIBRARY} ${QT_QTSQL_LIBRARY} )
    if ( BUILD_GTFS_REALTIME )
        set( LIBS ${LIBS} pthread protobuf ) # pthread is needed for protobuf
    endif ( BUILD_GTFS_REALTIME )
endif ( BUILD_PROVIDER_TYPE_GTFS )

# if ( BUILD_MARBLE_MAP )
#     message( "   - Build marble map" )
# Always add Marble library for TimetableMate?
    list ( APPEND LIBS marblewidget )
# endif ( BUILD_MARBLE_MAP )

target_link_libraries( timetablemate ${LIBS} )

install( TARGETS timetablemate ${INSTALL_TARGETS_DEFAULT_ARGS} )

# Generate completion data class files and HTML documentation from scripting.h.
# Rerun when the source file (scripting.h) or the tool changes.
# Put the generated class files into the binary directory
# and the HTML documentation into the sources.
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
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../script/scripting.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../../enums.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../../publictransportdataengine.h
            ${CMAKE_BINARY_DIR}/engine/timetablemate/completiongenerator/completiongenerator )

# Install configuration files
install( PROGRAMS timetablemate.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )
install( FILES timetablemate.kcfg DESTINATION ${KCFG_INSTALL_DIR} )
install( FILES timetablemateui.rc katepartui.rc DESTINATION ${DATA_INSTALL_DIR}/timetablemate )

### KAuth
kde4_install_auth_actions( org.kde.timetablemate timetablemate.actions )

kde4_add_executable( timetablematehelper timetablematehelper.cpp )
target_link_libraries( timetablematehelper
                       ${KDE4_KDECORE_LIBS} )
install( TARGETS timetablematehelper DESTINATION ${LIBEXEC_INSTALL_DIR} )
kde4_install_auth_helper_files( timetablematehelper org.kde.timetablemate root )
