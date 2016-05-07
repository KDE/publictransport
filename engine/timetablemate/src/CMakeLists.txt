
set( include_dirs
    
    ${PUBLICTRANSPORTHELPER_INCLUDES} # For "lib_config.h"
    ${CMAKE_CURRENT_BINARY_DIR} # For "debug_config.h"
)

if ( MARBLE_FOUND )
    list ( APPEND include_dirs ${MARBLE_INCLUDE_DIR} )
endif ( MARBLE_FOUND )

include_directories( ${include_dirs} )

# Create some variables to control debug output
option( ENABLE_DEBUGGER_EVENT "Enable debug output for eg. \"Execution started\", \"Interrupted\", \"Continued\", \"Aborted\", ..." OFF )
option( ENABLE_DEBUGGER_EVENT_POS_CHANGED "Enable debug output for every line of script code that was executed in the engine, warning: this can generate much output" OFF )
option( ENABLE_DEBUGGER_CONTROL "Enable debug output for control functions of the debugger, eg. continue(), debugStepOver(), ..." OFF )
option( ENABLE_DEBUGGER_STATE_CHANGE "Enable debug output for state changes of the debugger" OFF )
option( ENABLE_DEBUGGER_JOB_SYNCHRONIZATION "Enable debug output for job started/stopped/wait signals" OFF )

if ( ENABLE_DEBUGGER_EVENT )
    message( "     - Debug output for script debugger events" )
endif ( ENABLE_DEBUGGER_EVENT )
if ( ENABLE_DEBUGGER_EVENT_POS_CHANGED )
    message( "     - Debug output for every executed script code line, warning: can generate much output" )
endif ( ENABLE_DEBUGGER_EVENT_POS_CHANGED )
if ( ENABLE_DEBUGGER_CONTROL )
    message( "     - Debug output for script debugger control functions" )
endif ( ENABLE_DEBUGGER_CONTROL )
if ( ENABLE_DEBUGGER_STATE_CHANGE )
    message( "     - Debug output for script debugger state changes" )
endif ( ENABLE_DEBUGGER_STATE_CHANGE )
if ( ENABLE_DEBUGGER_JOB_SYNCHRONIZATION )
    message( "     - Debug output for script debugger job synchronization" )
endif ( ENABLE_DEBUGGER_JOB_SYNCHRONIZATION )

# Create debug_config.h from debug_config.h.in, ie. create #define's for each enabled variable from above
configure_file( debug_config.h.in debug_config.h )

# Generate completion data class files and HTML documentation from source code documentation.
# Gets rerun automatically when the source files change or completiongenerator gets rebuild.
# Output files are put into the current binary directory (file names need to match exactly,
# otherwise the command gets rerun every time).
add_custom_command( OUTPUT javascriptcompletiongeneric.h
                           javascriptcompletiongeneric.cpp
                           index.html
                           helper.html
                           network.html
                           networkrequest.html
                           resultobject.html
                           storage.html
                           datastreamprototype.html
                           enums.html
                           default.css
    COMMAND completiongenerator # --verbose
            --out_completion ${CMAKE_CURRENT_BINARY_DIR}
            --out_doc ${CMAKE_CURRENT_BINARY_DIR}
            --input_script "${CMAKE_CURRENT_SOURCE_DIR}/../../script/scriptapi.h"
            --input_script_doc "${CMAKE_CURRENT_SOURCE_DIR}/../../script/scriptapi-doc.h"
            --input_engine "${CMAKE_CURRENT_SOURCE_DIR}/../../engine-doc.h"
            --input_enum "${CMAKE_CURRENT_SOURCE_DIR}/../../enums.h"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../script/scriptapi.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../../script/scriptapi-doc.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../../enums.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../../engine-doc.h
            completiongenerator )

# FIXME Manually add file dependencies to wait until javascriptcompletiongeneric.h/.cpp are ready.
# Somehow it does not work without this and tries to build TimetableMate sources before
# completiongenerator is ready.
set_source_files_properties(
    javascriptcompletionmodel.cpp javascriptparser.cpp
    PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/javascriptcompletiongeneric.h )

qt5_wrap_cpp( timetablemate_MOC_SRCS ../../enums.h tabs/tabs.h )
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
   linkchecker.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/javascriptcompletiongeneric.h
    ${CMAKE_CURRENT_BINARY_DIR}/javascriptcompletiongeneric.cpp

   # Use files directly from the data engine
   ../../global.cpp
   ../../departureinfo.cpp
   ../../request.cpp
   ../../serviceprovider.cpp
   ../../serviceproviderdata.cpp
   ../../serviceproviderdatareader.cpp
   ../../serviceprovidertestdata.cpp
   ../../serviceproviderglobal.cpp
   ${timetablemate_MOC_SRCS}
)

if ( BUILD_PROVIDER_TYPE_SCRIPT )
    set ( timetablemate_SRCS ${timetablemate_SRCS}
        ../../script/serviceproviderscript.cpp
        ../../script/scriptapi.cpp
        ../../script/script_thread.cpp
        ../../script/scriptobjects.cpp
    )

    add_subdirectory( debugger )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

if ( BUILD_PROVIDER_TYPE_GTFS )
    set ( timetablemate_SRCS ${timetablemate_SRCS}
        ../../gtfs/gtfsdatabase.cpp
    )
endif ( BUILD_PROVIDER_TYPE_GTFS )

add_subdirectory( docks )
add_subdirectory( tabs )

ki18n_wrap_ui( timetablemate_SRCS timetablemateview_base.ui preferences.ui )
kconfig_add_kcfg_files( timetablemate_SRCS settings.kcfgc )
add_executable( timetablemate ${timetablemate_SRCS} )

add_dependencies( timetablemate publictransporthelper plasma_engine_publictransport )

# Collect all needed libraries in LIBS
set( LIBS 
    KF5::Parts # For the different main view tabs
    KF5::TextEditor # For the text editor main view tabs (for XML and script files)
    KF5::KDEWebKit # For the web main view tab
    KF5::Plasma # For the Plasma preview main view tab
    KF5::ThreadWeaver # For the debugger and for includes from the engine
    KF5::NewStuff # For plugin upload (publish)
    KF5::KDeclarative # For KDeclarative
    Qt5::Declarative # For the dashboard main view tabs
    publictransporthelper
    z # zlib
)

if ( BUILD_PROVIDER_TYPE_SCRIPT )
    set( LIBS ${LIBS} Qt5::Script )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

if ( BUILD_PROVIDER_TYPE_GTFS )
    set( LIBS ${LIBS} Qt5::Xml Qt5::Sql )
    if ( BUILD_GTFS_REALTIME )
        set( LIBS ${LIBS} ${PROTOBUF_LIBRARY} pthread ) # pthread is needed for protobuf
    endif ( BUILD_GTFS_REALTIME )
endif ( BUILD_PROVIDER_TYPE_GTFS )

if ( MARBLE_FOUND )
    message( "     - Build with marble widgets (eg. LatLonEdit)" )
    set ( LIBS ${LIBS} ${MARBLE_LIBRARIES} )
endif ( MARBLE_FOUND )

target_link_libraries( timetablemate ${LIBS} )

install( TARGETS timetablemate ${INSTALL_TARGETS_DEFAULT_ARGS} )

# Add unit tests
if ( BUILD_TESTS )
    add_subdirectory( tests )
endif ( BUILD_TESTS )

# Install configuration files
install( PROGRAMS timetablemate.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )
install( FILES timetablemate.kcfg DESTINATION ${KCFG_INSTALL_DIR} )
install( FILES timetablemateui.rc katepartui.rc DESTINATION ${DATA_INSTALL_DIR}/timetablemate )
install( FILES timetablemate.knsrc DESTINATION ${CONFIG_INSTALL_DIR} )

# Install QML files
install( FILES timetablemate.qml ActionButton.qml ActionToolButton.qml
         DESTINATION ${DATA_INSTALL_DIR}/timetablemate )

# Install generated documentation files, images get installed by ../doc/CMakeLists.txt
file( GLOB files "${CMAKE_CURRENT_BINARY_DIR}/*.html"
                 "${CMAKE_CURRENT_BINARY_DIR}/*.css" )
install( FILES ${files} DESTINATION ${DOCUMENTATION_DIR} )

### KAuth
kauth_install_actions( org.kde.timetablemate timetablemate.actions )

add_executable( timetablematehelper timetablematehelper.cpp )
target_link_libraries( timetablematehelper KF5::KDELibs4Support )
install( TARGETS timetablematehelper DESTINATION ${LIBEXEC_INSTALL_DIR} )
kauth_install_helper_files( timetablematehelper org.kde.timetablemate root )
