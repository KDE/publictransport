add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.kde.plasma.publictransport.engine\")

plasma_install_package(package org.kde.plasma.publictransport.engine)

set ( engineplasmoidplugin_SRCS
        plugin/engineplugin.cpp
        plugin/backend.cpp
        ../departureinfo.cpp
)

install (FILES plugin/qmldir DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/publictransport/engine)

add_library (engine SHARED ${engineplugin_SRCS})

target_link_libraries (engine
    Qt5::Core
    Qt5::Qml
    Qt5::Quick
    KF5::Plasma
)

install (TARGETS engineplugin DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/publictransport/engine)
