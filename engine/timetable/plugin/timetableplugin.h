#ifndef TIMETABLEPLUGIN_H
#define TIMETABLEPLUGIN_H

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class TimetablePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface");

public:
    virtual void registerTypes(const char *uri);
};

#endif // TIMETABLEPLUGIN_H