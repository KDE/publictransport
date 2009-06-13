#include "timetableaccessor_vrn.h"
#include <QRegExp>
#include <QDebug>

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
//     qDebug() << sType << ", " << sLine << ", " << sDirection << ", " << sDepHour;

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
// <tr>
// <td><img src="images/transparent.gif" style="width:5px;"></td>
// <td class="dm_content2" style="width:40px;">01:56</td>
// <td class="dm_content2" style="width:40px;" align="center"><img src="images/response/dm_train.gif" alt=""></td>
// <td class="dm_content2" nowrap>S4</td>
// <td class="dm_content2">Eppingen Bahnhof</td>
// </tr>
    return "(<td class=\".*\" style=\".*\">([0-9]{2})\\:([0-9]{2})</td>.*<td class=\".*\" style=\".*\" align=\".*\"><img src=\"images/response/(.*)\\..*\" alt=\".*\"></td>.*<td class=\".*\" nowrap>(.*)</td>.*<td class=\".*\">(.*)</td>)";
    // Matches: Departure Hour, Departure Minute, Product, Line, Target
}
