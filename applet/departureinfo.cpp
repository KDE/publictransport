
#include "departureinfo.h"

#include <QVariant>
#include <qmath.h>
#include <KLocale>
#include <KGlobal>

JourneyInfo::JourneyInfo( const QString &operatorName,
			  const QVariantList &vehicleTypesVariant,
			  const QDateTime &departure, const QDateTime &arrival,
			  const QString& pricing, const QString& startStopName,
			  const QString& targetStopName, int duration, int changes,
			  const QString &journeyNews, const QStringList &routeStops,
			  const QStringList &routeTransportLines,
			  const QStringList &routePlatformsDeparture,
			  const QStringList &routePlatformsArrival,
			  const QVariantList &routeVehicleTypesVariant,
			  const QList<QTime> &routeTimesDeparture,
			  const QList<QTime> &routeTimesArrival,
			  const QList<int> &routeTimesDepartureDelay,
			  const QList<int> &routeTimesArrivalDelay,
			  int routeExactStops ) {
    QList<VehicleType> vehicleTypes;
    foreach( QVariant vehicleTypeVariant, vehicleTypesVariant ) {
	VehicleType vehicleType = static_cast<VehicleType>( vehicleTypeVariant.toInt() );
	if ( !vehicleTypes.contains(vehicleType) )
	    vehicleTypes.append( vehicleType );
    }

    QList<VehicleType> routeVehicleTypes;
    foreach( QVariant routeVehicleType, routeVehicleTypesVariant )
	routeVehicleTypes.append( static_cast<VehicleType>(routeVehicleType.toInt()) );

    init( operatorName, vehicleTypes, departure, arrival, pricing, startStopName,
	  targetStopName, duration, changes, journeyNews, routeStops,
	  routeTransportLines, routePlatformsDeparture, routePlatformsArrival,
	  routeVehicleTypes, routeTimesDeparture, routeTimesArrival,
	  routeTimesDepartureDelay, routeTimesArrivalDelay, routeExactStops );
}

void JourneyInfo::init( const QString &operatorName,
			const QList< VehicleType > &vehicleTypes,
			const QDateTime& departure, const QDateTime& arrival,
			const QString& pricing, const QString& startStopName,
			const QString& targetStopName, int duration, int changes,
			const QString &journeyNews, const QStringList &routeStops,
			const QStringList &routeTransportLines,
			const QStringList &routePlatformsDeparture,
			const QStringList &routePlatformsArrival,
			const QList< VehicleType > &routeVehicleTypes,
			const QList<QTime> &routeTimesDeparture,
			const QList<QTime> &routeTimesArrival,
			const QList<int> &routeTimesDepartureDelay,
			const QList<int> &routeTimesArrivalDelay,
			int routeExactStops ) {
    m_operator = operatorName;
    m_vehicleTypes = vehicleTypes;
    m_departure = departure;
    m_arrival = arrival;
    m_pricing = pricing;
    m_startStopName = startStopName;
    m_targetStopName = targetStopName;
    m_duration = duration;
    m_changes = changes;
    m_journeyNews = journeyNews;
    m_routeStops = routeStops;
    m_routeTransportLines = routeTransportLines;
    m_routePlatformsDeparture = routePlatformsDeparture;
    m_routePlatformsArrival = routePlatformsArrival;
    m_routeVehicleTypes = routeVehicleTypes;
    m_routeTimesDeparture = routeTimesDeparture;
    m_routeTimesArrival = routeTimesArrival;
    m_routeTimesDepartureDelay = routeTimesDepartureDelay;
    m_routeTimesArrivalDelay = routeTimesArrivalDelay;
    m_routeExactStops = routeExactStops;
    
    generateHash();
}

QList< QVariant > JourneyInfo::vehicleTypesVariant() const {
    QList<QVariant> vehicleTypesVariant;
    foreach( VehicleType vehicleType, m_vehicleTypes )
	vehicleTypesVariant.append( static_cast<int>(vehicleType) );
    return vehicleTypesVariant;
}

QString JourneyInfo::durationToDepartureString( bool toArrival ) const {
    int totalSeconds = QDateTime::currentDateTime().secsTo(
	    toArrival ? m_arrival : m_departure );
    int totalMinutes = qCeil( (qreal)totalSeconds / 60.0 );
    if ( totalMinutes < 0 )
    	return i18n("depart is in the past");
    else
	return KGlobal::locale()->prettyFormatDuration( (ulong)(totalMinutes * 60000) );
// //     if ( -totalSeconds / 3600 >= 23 )
// // 	totalSeconds += 24 * 3600;

//     int seconds = totalSeconds % 60;
//     totalSeconds -= seconds;
//     if (seconds > 0)
// 	totalSeconds += 60;
    
//     if (totalSeconds < 0)
// 	return i18n("depart is in the past");

//     return Global::durationString(totalSeconds).replace(' ', "&nbsp;");
}


QString DepartureInfo::durationString () const {
    int totalSeconds = QDateTime::currentDateTime().secsTo( predictedDeparture() );
    int totalMinutes = qCeil( (qreal)totalSeconds / 60.0 );
    int remDelay = delayType() == Delayed ? m_delay : 0;
    if ( totalMinutes < 0 ) {
	if ( !(delayType() == Delayed && -totalMinutes > m_delay) )
	    return i18n("depart is in the past");
	else {
	    remDelay += totalMinutes;
	    if ( remDelay == 0 )
		return i18n("now");
	    else
		return "+ " + KGlobal::locale()->prettyFormatDuration( remDelay * 60000 );
	}
    }

    QString sDuration;
    if ( totalMinutes == 0 )
	sDuration = i18n("now");
    else
	sDuration = KGlobal::locale()->prettyFormatDuration( totalMinutes * 60000 )
			.replace( ' ', "&nbsp;" );
			
    if ( remDelay == 0 )
	return sDuration;
    else
	return sDuration + " + " + KGlobal::locale()->prettyFormatDuration( remDelay * 60000 );
// 	    + i18nc("+ remaining delay in minutes", "+ %1 minutes", remDelay );
    /*
    if ( -totalSeconds / 3600 >= 23 )
	totalSeconds += 24 * 3600;
    int seconds = totalSeconds % 60;
    totalSeconds -= seconds;
    if (seconds > 0)
	totalSeconds += 60;
    int remainingDelay = delayType() == Delayed ? delay : 0;
    if (totalSeconds < 0) {
	if (!(delayType() == Delayed && -totalSeconds > delay * 60))
	    return i18n("depart is in the past");
	else
	    remainingDelay -= qFloor( (float)totalSeconds / 60.0f );
    }

    int minutes = (totalSeconds / 60) % 60;
    int hours = totalSeconds / 3600;
    QString str;

    if ( delayType() == Delayed ) {
	if (hours > 0) {
	    if (minutes > 0)
		str = i18nc("h:mm + delay in minutes", "%1:%2 + %3 minutes", hours, QString("%1").arg(minutes, 2, 10, QLatin1Char('0')), remainingDelay);
	    else
		str = i18ncp("Hour(s) + delay in minutes", "%1 hour + %2 minutes", "in %1 hours + %2 minutes", hours, remainingDelay);
	} else if (minutes > 0)
	    str = i18nc("Minute(s) + delay in minutes", "%1 + %2 minutes", minutes, remainingDelay);
	else if (hours == 0 && minutes == 0)
	    str = i18nc("Now + delay in minutes", "now + %1 minutes", remainingDelay );
	else
	    str = i18nc("+ remaining delay in minutes", "+ %1 minutes", remainingDelay );
    } else {
	if (hours > 0) {
	    if (minutes > 0)
		str = i18nc("h:mm", "%1:%2 hours", hours, QString("%1").arg(minutes, 2, 10, QLatin1Char('0')));
	    else
		str = i18np("%1 hour", "%1 hours", hours);
	}
	else if (minutes > 0)
	    str = i18np("%1 minute", "%1 minutes", minutes);
	else
	    str = i18n("now");
    }

    return str.replace( ' ', "&nbsp;" );*/
}

void DepartureInfo::init( const QString &operatorName, const QString &line,
			  const QString &target, const QDateTime &departure,
			  VehicleType lineType, LineServices lineServices,
			  const QString &platform, int delay,
			  const QString &delayReason, const QString &journeyNews,
			  const QStringList &routeStops,
			  const QList<QTime> &routeTimes,
			  int routeExactStops ) {
    m_visible = true;
    
    QRegExp rx ( "[0-9]*$" );
    rx.indexIn ( line );
    if ( rx.isValid() )
	m_lineNumber = rx.cap().toInt();
    else
	m_lineNumber = 0;

    m_operator = operatorName;
    m_lineString = line;
    m_target = target;
    m_departure = departure;
    m_vehicleType = lineType;
    m_lineServices = lineServices;
    m_platform = platform;
    m_delay = delay;
    m_delayReason = delayReason;
    m_journeyNews = journeyNews;

    m_routeStops = routeStops;
    m_routeTimes = routeTimes;
    m_routeExactStops = routeExactStops;

    generateHash();
}

void DepartureInfo::generateHash() {
    m_hash = QString( "%1%2%3" ).arg( m_departure.toString("hhmm") )
				.arg( static_cast<int>(m_vehicleType) )
				.arg( m_lineString );
}

void JourneyInfo::generateHash() {
    QString vehicles;
    foreach ( int vehicleType, m_vehicleTypes )
	vehicles += QString::number( static_cast<int>(vehicleType) );
    m_hash = QString( "%1%2%3" ).arg( m_departure.toString("hhmm") )
				.arg( m_duration ).arg( m_changes )
				.arg( vehicles );
}

bool operator< ( const JourneyInfo& ji1, const JourneyInfo& ji2 ) {
    return ji1.departure() < ji2.departure();
}

bool operator < ( const DepartureInfo &di1, const DepartureInfo &di2 ) {
    return di1.predictedDeparture() < di2.predictedDeparture();
}
