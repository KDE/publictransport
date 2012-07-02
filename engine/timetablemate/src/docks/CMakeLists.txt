
set ( docks_SRCS
    docks/docktoolbar.cpp
    docks/abstractdockwidget.cpp
    docks/projectsdockwidget.cpp
    docks/documentationdockwidget.cpp
    docks/testdockwidget.cpp
    docks/webinspectordockwidget.cpp
    docks/networkmonitordockwidget.cpp
)

if ( BUILD_PROVIDER_TYPE_SCRIPT )
    set ( docks_SRCS ${docks_SRCS}
        docks/variablesdockwidget.cpp
        docks/breakpointdockwidget.cpp
        docks/backtracedockwidget.cpp
        docks/consoledockwidget.cpp
        docks/outputdockwidget.cpp
    )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

# Add sources of this directory to the sources list of the parent directories CMakeLists.txt
set( timetablemate_SRCS ${timetablemate_SRCS} ${docks_SRCS} PARENT_SCOPE )
