
add_definitions (${QT_DEFINITIONS})

include_directories(
   ${CMAKE_CURRENT_SOURCE_DIR}/../
   ${CMAKE_CURRENT_BINARY_DIR}/../ # For config.h, javascriptcompletiongeneric.h
   ${CMAKE_CURRENT_BINARY_DIR}/../../../ # For config.h
   ${MARBLE_INCLUDE_DIR}
)

if ( BUILD_PROVIDER_TYPE_SCRIPT )
    set_source_files_properties( ${CMAKE_CURRENT_BINARY_DIR}/../javascriptcompletiongeneric.cpp
                                 ${CMAKE_CURRENT_BINARY_DIR}/../javascriptcompletiongeneric.h
                                 PROPERTIES GENERATED TRUE )

    set( JavaScriptParserTest_SRCS
        JavaScriptParserTest.cpp
        # Use files directly from the data engine
        ../javascriptparser.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/../javascriptcompletiongeneric.h
        ${CMAKE_CURRENT_BINARY_DIR}/../javascriptcompletiongeneric.cpp
    )
    add_executable( JavaScriptParserTest ${JavaScriptParserTest_SRCS} )
    add_test( JavaScriptParserTest JavaScriptParserTest )
    target_link_libraries( JavaScriptParserTest
        Qt5::Test Qt5::Gui KF5::KDELibs4Support )

    # Needed to have javascriptcompletiongeneric.h/.cpp generated before building the test
    add_dependencies( JavaScriptParserTest timetablemate plasma_engine_publictransport )

    # Run moc manually, alternatively add #include "moc_foo.cpp" to the end of the source files
    qt5_wrap_cpp( timetablemate_test_MOC_SRCS
        ../project.h
        ../projectmodel.h
        ../networkmonitormodel.h
        ../testmodel.h
        ../javascriptmodel.h
        ../javascriptcompletionmodel.h
        ../changelogwidget.h
        ../linkchecker.h

        ../debugger/debugger.h
        ../debugger/debuggeragent.h
        ../debugger/debuggerjobs.h
        ../debugger/timetabledatarequestjob.h
        ../debugger/backtracemodel.h
        ../debugger/breakpointmodel.h
        ../debugger/variablemodel.h

        ../tabs/tabs.h
        ../tabs/abstracttab.h
        ../tabs/projectsourcetab.h
        ../tabs/scripttab.h
        ../tabs/webtab.h
        ../tabs/dashboardtab.h
        ../tabs/plasmapreviewtab.h
        ../tabs/plasmapreview.h
        ../tabs/gtfsdatabasetab.h

        ../../../serviceprovider.h
        ../../../serviceproviderdata.h

        ../../../script/script_thread.h
        ../../../script/serviceproviderscript.h

        ../../../enums.h

        ${CMAKE_CURRENT_BINARY_DIR}/../javascriptcompletiongeneric.h
    )
    set( DebuggerTest_SRCS
        DebuggerTest.cpp

        # Use files directly from TimetableMate
        ../project.cpp
        ../projectmodel.cpp
        ../networkmonitormodel.cpp
        ../testmodel.cpp
        ../serviceproviderdatawriter.cpp
        ../serviceproviderdatatester.cpp
        ../javascriptcompletionmodel.cpp
        ../javascriptmodel.cpp
        ../javascriptparser.cpp
        ../projectsettingsdialog.cpp
        ../changelogwidget.cpp
        ../linkchecker.cpp
        ../debugger/debugger.cpp
        ../debugger/debuggeragent.cpp
        ../debugger/variablemodel.cpp
        ../debugger/backtracemodel.cpp
        ../debugger/breakpointmodel.cpp
        ../debugger/debuggerjobs.cpp
        ../debugger/timetabledatarequestjob.cpp
        ../debugger/debuggerstructures.cpp
        ../tabs/abstracttab.cpp
        ../tabs/projectsourcetab.cpp
        ../tabs/scripttab.cpp
        ../tabs/projectsourcetab.cpp
        ../tabs/dashboardtab.cpp
        ../tabs/webtab.cpp
        ../tabs/plasmapreviewtab.cpp
        ../tabs/plasmapreview.cpp
        ../tabs/gtfsdatabasetab.cpp

        # Use files directly from the data engine
        ../../../global.cpp
        ../../../departureinfo.cpp
        ../../../request.cpp
        ../../../serviceprovider.cpp
        ../../../serviceproviderdata.cpp
        ../../../serviceproviderdatareader.cpp
        ../../../serviceprovidertestdata.cpp
        ../../../serviceproviderglobal.cpp

        ../../../script/scriptobjects.cpp
        ../../../script/script_thread.cpp
        ../../../script/scriptapi.cpp
        ../../../script/serviceproviderscript.cpp

        ../../../gtfs/gtfsdatabase.cpp

        ${CMAKE_CURRENT_BINARY_DIR}/../javascriptcompletiongeneric.h
        ${CMAKE_CURRENT_BINARY_DIR}/../javascriptcompletiongeneric.cpp
        ${timetablemate_test_MOC_SRCS}
    )

    set( LIBS
        Qt5::Test Qt5::Gui KF5::KDELibs4Support KF5::KIOCore
        KF5::TextEditor # For the text editor main view tabs (for XML and script files)
        KF5::KDEWebKit # For the web main view tab
        KF5::Plasma # For the Plasma preview main view tab
        KF5::ThreadWeaver # For the debugger and for includes from the engine
        KF5::NewStuff # For plugin upload (publish)
        Qt5::Declarative # For the dashboard main view tabs
        KF5::KDeclarative # For KDeclarative
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

    add_executable( DebuggerTest ${DebuggerTest_SRCS} )
    add_test( DebuggerTest DebuggerTest )
    target_link_libraries( DebuggerTest ${LIBS} )

    # Needed to have javascriptcompletiongeneric.h/.cpp generated before building the test
    add_dependencies( DebuggerTest timetablemate plasma_engine_publictransport )

endif ( BUILD_PROVIDER_TYPE_SCRIPT )
