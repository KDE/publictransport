#include "backend.h"
#include "engineplugin.h"

#include <QtQml>

void EngiePlugin::registerTypes(const char *uri)
{
    Q_ASSERT( uri == QLatin1String("org.kde.plasma.engineplugin") );
    qmlRegisterType<Backend>( uri, 0, 1, "Backend" );
    qmlRegisterType<PublicTransportInfo>( uri, 0, 1, "PublicTransportInfo");
    qmlRegisterType<DepartureInfo>( uri, 0, 1, "DepartureInfo");
    qmlRegisterType<StopInfo>( uri, 0, 1, "StopInfo" );
    qmlRegisterType<JourneyInfo>( uri, 0, 1, "JourneyInfo" );
}
