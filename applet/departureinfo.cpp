
#include "departureinfo.h"

#include <QVariant>
#include <qmath.h>
#include <KLocale>
#include <KGlobal>
#include <KDebug>
#include <Plasma/Theme>
#include <KColorUtils>

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
}

QString DepartureInfo::delayString() const {
    return delayType() == Delayed ? QString(" (+%1)").arg(m_delay) : QString();
}

QString DepartureInfo::durationString( bool showDelay ) const {
    int totalSeconds = QDateTime::currentDateTime().secsTo( predictedDeparture() );
    int totalMinutes = qCeil( (qreal)totalSeconds / 60.0 );
    if ( totalMinutes < 0 )
	return i18n("depart is in the past");

    QString sDuration;
    if ( totalMinutes == 0 )
	sDuration = i18n("now");
    else
	sDuration = KGlobal::locale()->prettyFormatDuration( totalMinutes * 60000 );

    return showDelay ? sDuration + delayString() : sDuration;
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

QString DepartureInfo::delayText() const {
    QString sText;
    DelayType type = delayType();
    if ( type == OnSchedule ) {
	sText = i18nc("A public transport vehicle departs on schedule", "On schedule");
	sText = sText.prepend( QString("<span style='color:%1;'>")
		     .arg(Global::textColorOnSchedule().name()) ).append( "</span>" );
    } else if ( type == Delayed ) {
	sText = i18np("+%1 minute", "+%1 minutes", delay());
	sText = sText.replace( QRegExp("(+?\\s*\\d+)"),
		QString("<span style='color:%1;'>+&nbsp;\\1</span>")
		.arg(Global::textColorDelayed().name()) );

	if ( !delayReason().isEmpty() ) {
	    sText += ", " + delayReason();
	}
    } else { // DelayUnknown:
	sText = i18n("No information available");
    }
    
    return sText;
}

bool operator< ( const JourneyInfo& ji1, const JourneyInfo& ji2 ) {
    return ji1.departure() < ji2.departure();
}

bool operator < ( const DepartureInfo &di1, const DepartureInfo &di2 ) {
    return di1.predictedDeparture() < di2.predictedDeparture();
}
