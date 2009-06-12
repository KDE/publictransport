#include "timetableaccessor_efa.h"
#include <QRegExp>
#include <QDebug>

QList< DepartureInfo > TimetableAccessorEfa::parseDocument( const QString &document )
{
    QList< DepartureInfo > journeys;

    qDebug() << "TimetableAccessorEfa::parseDocument" << "Parsing...";
    QRegExp rx(regExpSearch(), Qt::CaseInsensitive);
    rx.setMinimal(true);
    int pos = 0;
    while ((pos = rx.indexIn(document, pos)) != -1) {
        if (!rx.isValid()) {
            qDebug() << "TimetableAccessorEfa::parseDocument" << "Parse error";
            continue; // parse error
        }

        journeys.append( getInfo(rx) );
        pos  += rx.matchedLength();
    }

    return journeys;
}

QString TimetableAccessorEfa::rawUrl()
{
    return "";
}

QString TimetableAccessorEfa::regExpSearch()
{
    return "";
}

DepartureInfo TimetableAccessorEfa::getInfo(QRegExp)
{
    return DepartureInfo();
}
