#include "timetableaccessor_html.h"
#include <QRegExp>
#include <QDebug>
#include "timetableaccessor_html_infos.h"

TimetableAccessorHtml::TimetableAccessorHtml()
{
    m_info = TimetableAccessorInfo();
}

TimetableAccessorHtml::TimetableAccessorHtml ( ServiceProvider serviceProvider )
    : TimetableAccessor ()
{
    switch ( serviceProvider )
    {
	case BVG:
	    m_info = TimetableAccessorInfo_Bvg();
	    break;

	case DVB:
	    m_info = TimetableAccessorInfo_Dvb();
	    break;

	case Fahrplaner:
	    m_info = TimetableAccessorInfo_Fahrplaner();
	    break;

	case IMHD:
	    m_info = TimetableAccessorInfo_Imhd();
	    break;

	case NASA:
	    m_info = TimetableAccessorInfo_Nasa();
	    break;

	case VRN:
	    m_info = TimetableAccessorInfo_Vrn();
	    break;

	case VVS:
	    m_info = TimetableAccessorInfo_Vvs();
	    break;
    }
}

QList< DepartureInfo > TimetableAccessorHtml::parseDocument( const QString &document )
{
    QList< DepartureInfo > journeys;

    qDebug() << "TimetableAccessorHtml::parseDocument" << "Parsing...";
    QRegExp rx(regExpSearch(), Qt::CaseInsensitive);
    rx.setMinimal(true);
    int pos = 0;
    while ((pos = rx.indexIn(document, pos)) != -1) {
        if (!rx.isValid()) {
            qDebug() << "TimetableAccessorHtml::parseDocument" << "Parse error";
            continue; // parse error
        }

	QList< TimetableInformation > infos = regExpInfos();
	if ( infos.isEmpty() )
	    journeys.append( getInfo(rx) );
	else
	{	    
	    QString sDepHour, sDepMinute, sType, sLine, sDirection;
	    if ( infos.contains(DepartureHour) )
		sDepHour = rx.cap( infos.indexOf( DepartureHour ) + 1 );
	    if ( infos.contains(DepartureMinute) )
		sDepMinute = rx.cap( infos.indexOf( DepartureMinute ) + 1 );
	    if ( infos.contains(TypeOfVehicle) )
		sType = rx.cap( infos.indexOf( TypeOfVehicle ) + 1 );
	    if ( infos.contains(TransportLine) )
		sLine = rx.cap( infos.indexOf( TransportLine ) + 1 ).trimmed();
	    if ( infos.contains(Direction) )
		sDirection = rx.cap( infos.indexOf( Direction ) + 1 );
	    
	    journeys.append( DepartureInfo( sLine, DepartureInfo::getLineTypeFromString(sType), sDirection, QTime(sDepHour.toInt(), sDepMinute.toInt()), sLine.isEmpty() ? false : sLine.at(0) == 'N' ) );
	}
        pos  += rx.matchedLength();
    }

    return journeys;
}

QString TimetableAccessorHtml::rawUrl()
{
    return m_info.rawUrl;
}

QString TimetableAccessorHtml::regExpSearch()
{
    return m_info.regExpSearch;
}

QList< TimetableInformation > TimetableAccessorHtml::regExpInfos()
{
    return m_info.regExpInfos;
}

DepartureInfo TimetableAccessorHtml::getInfo(QRegExp)
{
    return DepartureInfo();
}
