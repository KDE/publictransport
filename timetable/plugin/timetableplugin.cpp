#include "timetableplugin.h"
#include "timetablehelper.h"
#include "timetablebackend.h"

#include <QtQml>

void TimetablePlugin::registerTypes(const char *uri)
{
    Q_ASSERT( uri == QLatin1String("org.kde.plasma.private.publictransport.timetable") );
    qmlRegisterType<TimetableHelper>( uri, 0, 1, "TimetableHelper" );
    qmlRegisterType<TimetableBackend>( uri, 0, 1, "TimetableBackend");
}
