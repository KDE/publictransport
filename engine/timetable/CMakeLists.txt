add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.kde.plasma.publictransport.timetable\")

plasma_install_package(plasmoid org.kde.plasma.publictransport.timetable)

set ( timetable_SRCS
        plugin/timetableplugin.cpp
        plugin/timetablehelper.cpp
)

install (FILES plugin/qmldir DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/private/publictransport/timetable)

add_library (timetable SHARED ${timetable_SRCS})

kcoreaddons_desktop_to_json(timetable plasmoid/metadata.desktop)

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

install (TARGETS timetable DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/private/publictransport/timetable)
