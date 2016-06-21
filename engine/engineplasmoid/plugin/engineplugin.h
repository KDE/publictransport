#ifndef ENGINEPLUGIN_H
#define ENGINEPLUGIN_H

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class EnginePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionPlugin");

public:
    virtual void registerTypes(const char *uri);
};

#endif // ENGINEPLUGIN_H
