#include "backend.h"
#include "engineplugin.h"

#include <QtQml>

void TimetablePlugin::registerTypes(const char *uri)
{
    Q_ASSERT( uri == QLatin1String("org.kde.plasma.timetableplugin") );
    qmlRegisterType<TimetableHelper>( uri, 0, 1, "TimetableHelper");
}
