
# Add sources of this directory to the sources list of the parent directories CMakeLists.txt
set( timetablemate_SRCS
    ${timetablemate_SRCS}

    tabs/abstracttab.cpp
    tabs/overviewtab.cpp
    tabs/projectsourcetab.cpp
    tabs/scripttab.cpp
    tabs/webtab.cpp
    tabs/plasmapreviewtab.cpp
    tabs/plasmapreview.cpp

    PARENT_SCOPE
)

# Install QML files
install( FILES overviewtab.qml ActionButton.qml ActionToolButton.qml DESTINATION ${DATA_INSTALL_DIR}/timetablemate )
