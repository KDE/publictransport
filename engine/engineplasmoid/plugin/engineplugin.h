#ifndef ENGINEPLUGIN_H
#define ENGINEPLUGIN_H

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class EnginePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface");

public:
    virtual void registerTypes(const char *uri);
    //virtual void initializeEngine(QQmlEngine *engine, const char *uri);
};

#endif // ENGINEPLUGIN_H
