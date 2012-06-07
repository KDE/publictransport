
# Add sources of this directory to the sources list of the parent directories CMakeLists.txt
set( timetablemate_SRCS
    ${timetablemate_SRCS}

    docks/docktoolbar.cpp
    docks/abstractdockwidget.cpp
    docks/projectsdockwidget.cpp
    docks/documentationdockwidget.cpp
    docks/consoledockwidget.cpp
    docks/outputdockwidget.cpp
    docks/variablesdockwidget.cpp
    docks/breakpointdockwidget.cpp
    docks/backtracedockwidget.cpp
    docks/testdockwidget.cpp
    docks/webinspectordockwidget.cpp
    docks/networkmonitordockwidget.cpp

    PARENT_SCOPE
)

