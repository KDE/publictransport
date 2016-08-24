add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.kde.plasma.publictransport.timetable\")

plasma_install_package(package org.kde.plasma.publictransport.timetable)

set ( timetableplugin_SRCS
        plugin/timetableplugin.cpp
        plugin/timetablehelper.cpp
)

install (FILES plugin/qmldir DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/publictransport/timetable)

add_library (engine SHARED ${timetableplugin_SRCS})

target_link_libraries (timetable
    Qt5::Core
    Qt5::Qml
    Qt5::Quick
    KF5::Plasma
)

install (TARGETS timetableplugin DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/publictransport/timetable)
