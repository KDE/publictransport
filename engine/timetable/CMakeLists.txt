add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.kde.plasma.publictransport.timetable\")

set (timetable_SRCS timetableapplet.cpp)

add_library (timetable MODULE ${timetable_SRCS})

target_link_libraries (timetable
    Qt5::Core
    Qt5::Qml
    Qt5::Quick
    Qt5::Widgets
    KF5::Plasma
    KF5::NewStuff
    KF5::CoreAddons
    KF5::KDELibs4Support
)

kcoreaddons_desktop_to_json(timetable package/metadata.desktop)

install (TARGETS timetable DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/publictransport/timetable)
install (FILES package/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install (FILES publictransport.knsrc DESTINATION ${CONFIG_INSTALL_DIR})

plasma_install_package(package org.kde.plasma.publictransport.timetable)