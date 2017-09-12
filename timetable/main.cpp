#include <QUrl>
#include <QApplication>
#include <QQmlApplicationEngine>

// #include "plugin/timetableplugin.h"
// #include "plugin/timetablehelper.h"
// #include "plugin/timetablebackend.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName("KDE");
    QCoreApplication::setOrganizationDomain("kde.org");
    QCoreApplication::setApplicationName("Public transport timetable");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:///main.qml")));

    return app.exec();
}
