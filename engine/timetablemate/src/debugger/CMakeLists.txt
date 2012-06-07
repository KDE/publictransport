
# Add sources of this directory to the sources list of the parent directories CMakeLists.txt
set( timetablemate_SRCS
    ${timetablemate_SRCS}

    debugger/debugger.cpp
    debugger/debuggerstructures.cpp
    debugger/debuggeragent.cpp
    debugger/debuggerjobs.cpp
    debugger/timetabledatarequestjob.cpp
    debugger/breakpointmodel.cpp
    debugger/backtracemodel.cpp
    debugger/variablemodel.cpp

    PARENT_SCOPE
)
