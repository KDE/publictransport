
set( tab_SRCS
    tabs/abstracttab.cpp
    tabs/dashboardtab.cpp
    tabs/projectsourcetab.cpp
    tabs/webtab.cpp
    tabs/plasmapreviewtab.cpp
    tabs/plasmapreview.cpp
)

if ( BUILD_PROVIDER_TYPE_SCRIPT )
    set( tab_SRCS ${tab_SRCS} tabs/scripttab.cpp )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

# Add sources of this directory to the sources list of the parent directory's CMakeLists.txt
set( timetablemate_SRCS ${timetablemate_SRCS} ${tab_SRCS} PARENT_SCOPE )

# Install QML files
install( FILES dashboard.qml ActionButton.qml ActionToolButton.qml
         DESTINATION ${DATA_INSTALL_DIR}/timetablemate )
