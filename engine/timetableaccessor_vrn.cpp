#include "timetableaccessor_vrn.h"
#include <QRegExp>
#include <QDebug>
/*
QList< DepartureInfo > TimetableAccessorVrn::parseDocument( const QString &document )
{
    QList< DepartureInfo > journeys;*/
   // QString sFind = "(<td class=\".*\" style=\".*\">([0-9]{2})\\:([0-9]{2})</td>.*<td class=\".*\" style=\".*\" align=\".*\"><img src=\"images/response/(.*)\\..*\" alt=\".*\"></td>.*<td class=\".*\" nowrap>(.*)</td>.*<td class=\".*\">(.*)</td>)";
    // Matches: Departure Hour, Departure Minute, Product, Line, Target
/*
    qDebug() << "Parsing...";
    QRegExp rx(sFind, Qt::CaseInsensitive);
    rx.setMinimal(true);
    int pos = 0;
    while ((pos = rx.indexIn(document, pos)) != -1) {
        if (!rx.isValid()) {
            qDebug() << "Parse error";
            continue; // parse error
        }

	QString sType = rx.cap( 4 );
        QString sLine = rx.cap( 5 );
        QString sTarget = rx.cap( 6 );
        QString sDepHour = rx.cap( 2 );
        QString sDepMinute = rx.cap( 3 );
        QRegExp rxNumber("[0-9]+");
        rxNumber.indexIn(sLine);
        int iLine = rxNumber.cap().toInt();
        qDebug() << sType << ", " << sLine << ", " << sTarget << ", " << sDepHour;

        DepartureInfo::LineType lineType = DepartureInfo::Unknown;
        if ( sType == "U-Bahn" )
            lineType = DepartureInfo::Subway;
        else if ( sType == "dm_train" )
            lineType = DepartureInfo::Tram;
        else if ( sType == "dm_bus" )
            lineType = DepartureInfo::Bus;
        DepartureInfo departureInfo(lineType, iLine, false, sTarget, QTime(sDepHour.toInt(), sDepMinute.toInt()));
        departureInfo.setLineString(sLine);
        journeys.append( departureInfo );

        pos  += rx.matchedLength();
    }

    return journeys;
}*/

QString TimetableAccessorVrn::rawUrl()
{
    return "http://efa9.vrn.de/vrn/XSLT_DM_REQUEST?language=de&type_dm=stop&mode=direct&place_dm=%1&name_dm=%2&deleteAssignedStops=1";
}

DepartureInfo TimetableAccessorVrn::getInfo ( QRegExp rx )
{
    QString sType = rx.cap( 4 );
    QString sLine = rx.cap( 5 );
    QString sDirection = rx.cap( 6 );
    QString sDepHour = rx.cap( 2 );
    QString sDepMinute = rx.cap( 3 );
    QRegExp rxNumber("[0-9]+");
    rxNumber.indexIn(sLine);
    int iLine = rxNumber.cap().toInt();
    qDebug() << sType << ", " << sLine << ", " << sDirection << ", " << sDepHour;

    LineType lineType = Unknown;
    if ( sType == "U-Bahn" )
	lineType = Subway;
    else if ( sType == "dm_train" )
	lineType = Tram;
    else if ( sType == "dm_bus" )
	lineType = Bus;
    return DepartureInfo( sLine, lineType, sDirection, QTime(sDepHour.toInt(), sDepMinute.toInt()) );
}

QString TimetableAccessorVrn::regExpSearch()
{
    return "(<td class=\".*\" style=\".*\">([0-9]{2})\\:([0-9]{2})</td>.*<td class=\".*\" style=\".*\" align=\".*\"><img src=\"images/response/(.*)\\..*\" alt=\".*\"></td>.*<td class=\".*\" nowrap>(.*)</td>.*<td class=\".*\">(.*)</td>)";
    // Matches: Departure Hour, Departure Minute, Product, Line, Target
}
