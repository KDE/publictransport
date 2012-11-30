
set( tab_SRCS
    tabs/abstracttab.cpp
    tabs/dashboardtab.cpp
    tabs/projectsourcetab.cpp
    tabs/webtab.cpp
    tabs/plasmapreviewtab.cpp
    tabs/plasmapreview.cpp
    tabs/tabs.h
)

if ( BUILD_PROVIDER_TYPE_SCRIPT )
    set( tab_SRCS ${tab_SRCS} tabs/scripttab.cpp )
endif ( BUILD_PROVIDER_TYPE_SCRIPT )

if ( BUILD_PROVIDER_TYPE_GTFS )
    set( tab_SRCS ${tab_SRCS} tabs/gtfsdatabasetab.cpp )
endif ( BUILD_PROVIDER_TYPE_GTFS )

# Add sources of this directory to the sources list of the parent directory's CMakeLists.txt
set( timetablemate_SRCS ${timetablemate_SRCS} ${tab_SRCS} PARENT_SCOPE )

# Install QML files
install( FILES dashboard.qml ActionButton.qml ActionToolButton.qml
         DESTINATION ${DATA_INSTALL_DIR}/timetablemate )

if ( BUILD_PROVIDER_TYPE_GTFS )
    install( FILES gtfs_dashboard.qml
            DESTINATION ${DATA_INSTALL_DIR}/timetablemate )
endif ( BUILD_PROVIDER_TYPE_GTFS )
