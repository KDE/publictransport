install (FILES timetable.knsrc DESTINATION ${CONFIG_INSTALL_DIR})
install (PROGRAMS timetable.desktop DESTINATION ${KDE_INSTALL_APPDIR})

qt5_add_resources(RESOURCES timetable.qrc)

add_executable (timetable main.cpp ${RESOURCES})

target_link_libraries (timetable
    Qt5::Core
    Qt5::Qml
    Qt5::Quick
    Qt5::Widgets
    KF5::Plasma
    KF5::NewStuff
    KF5::KDELibs4Support
)

install (TARGETS timetable DESTINATION ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})
