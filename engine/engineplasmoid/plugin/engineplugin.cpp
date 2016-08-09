#include "backend.h"
#include "engineplugin.h"

#include <QtQml>

void EnginePlugin::registerTypes(const char *uri)
{
    Q_ASSERT( uri == QLatin1String("org.kde.plasma.engineplugin") );
    qmlRegisterType<PublicTransportEngine>( uri, 0, 1, "PublicTransportEngine");
}

//void EnginePlugin::initializeEngine(QQmlEngine *engine, const char *uri)
//{
//}
