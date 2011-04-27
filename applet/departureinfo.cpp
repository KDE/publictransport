/*
*   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU Library General Public License as
*   published by the Free Software Foundation; either version 2 or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details
*
*   You should have received a copy of the GNU Library General Public
*   License along with this program; if not, write to the
*   Free Software Foundation, Inc.,
*   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "departureinfo.h"

#include <QVariant>
#include <qmath.h>
#include <KLocale>
#include <KGlobal>
#include <KDebug>
#include <Plasma/Theme>
#include <KColorUtils>

JourneyInfo::JourneyInfo( const QString &operatorName, const QVariantList &vehicleTypesVariant,
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
                        int routeExactStops ) : PublicTransportInfo()
{
    QSet<VehicleType> vehicleTypes;
    foreach( const QVariant &vehicleTypeVariant, vehicleTypesVariant ) {
        VehicleType vehicleType = static_cast<VehicleType>( vehicleTypeVariant.toInt() );
        if ( !vehicleTypes.contains( vehicleType ) ) {
            vehicleTypes << vehicleType;
        }
    }

    QList<VehicleType> routeVehicleTypes;
    foreach( const QVariant &routeVehicleType, routeVehicleTypesVariant )
    routeVehicleTypes.append( static_cast<VehicleType>( routeVehicleType.toInt() ) );

    init( operatorName, vehicleTypes, departure, arrival, pricing, startStopName,
        targetStopName, duration, changes, journeyNews, routeStops,
        routeTransportLines, routePlatformsDeparture, routePlatformsArrival,
        routeVehicleTypes, routeTimesDeparture, routeTimesArrival,
        routeTimesDepartureDelay, routeTimesArrivalDelay, routeExactStops );
}

void JourneyInfo::init( const QString &operatorName, const QSet< VehicleType > &vehicleTypes,
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
                        int routeExactStops )
{
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

QString JourneyInfo::departureText( bool htmlFormatted, bool displayTimeBold,
                                    bool showRemainingMinutes, bool showDepartureTime,
                                    int linesPerRow ) const
{
    QString sTime, sDeparture = departure().toString( "hh:mm" );
    if ( htmlFormatted && displayTimeBold ) {
        sDeparture = sDeparture.prepend( "<span style='font-weight:bold;'>" ).append( "</span>" );
    }

    if ( departure().date() != QDate::currentDate() ) {
        sDeparture += ", " + DepartureInfo::formatDateFancyFuture( departure().date() );
    }

    if ( showDepartureTime && showRemainingMinutes ) {
        QString sText = durationToDepartureString();
        if ( htmlFormatted ) {
            sText = sText.replace( QRegExp( "\\+(?:\\s*|&nbsp;)(\\d+)" ),
                                QString( "<span style='color:%1;'>+&nbsp;\\1</span>" )
                                .arg( GlobalApplet::textColorDelayed().name() ) );
        }

        if ( linesPerRow > 1 ) {
            sTime = QString( htmlFormatted ? "%1<br>(%2)" : "%1\n(%2)" )
                    .arg( sDeparture ).arg( sText );
        } else {
            sTime = QString( "%1 (%2)" ).arg( sDeparture ).arg( sText );
        }
    } else if ( showDepartureTime ) {
        sTime = sDeparture;
    } else if ( showRemainingMinutes ) {
        sTime = durationToDepartureString();
        if ( htmlFormatted ) {
            sTime = sTime.replace( QRegExp( "\\+(?:\\s*|&nbsp;)(\\d+)" ),
                                QString( "<span style='color:%1;'>+&nbsp;\\1</span>" )
                                .arg( GlobalApplet::textColorDelayed().name() ) );
        }
    } else {
        sTime.clear();
    }

    return sTime;
}

QString JourneyInfo::arrivalText( bool /*htmlFormatted*/, bool displayTimeBold,
                                bool showRemainingMinutes, bool showDepartureTime,
                                int linesPerRow ) const
{
    QString sTime, sArrival = arrival().toString( "hh:mm" );
    if ( displayTimeBold ) {
        sArrival = sArrival.prepend( "<span style='font-weight:bold;'>" ).append( "</span>" );
    }

    if ( arrival().date() != QDate::currentDate() ) {
        sArrival += ", " + DepartureInfo::formatDateFancyFuture( arrival().date() );
    }

    if ( showDepartureTime && showRemainingMinutes ) {
        QString sText = durationToDepartureString( true );
        sText = sText.replace( QRegExp( "\\+(?:\\s*|&nbsp;)(\\d+)" ),
                            QString( "<span style='color:%1;'>+&nbsp;\\1</span>" )
                            .arg( GlobalApplet::textColorDelayed().name() ) );

        if ( linesPerRow > 1 ) {
            sTime = QString( "%1<br>(%2)" ).arg( sArrival ).arg( sText );
        } else {
            sTime = QString( "%1 (%2)" ).arg( sArrival ).arg( sText );
        }
    } else if ( showDepartureTime ) {
        sTime = sArrival;
    } else if ( showRemainingMinutes ) {
        sTime = durationToDepartureString( true );
        sTime = sTime.replace( QRegExp( "\\+(?:\\s*|&nbsp;)(\\d+)" ),
                            QString( "<span style='color:%1;'>+&nbsp;\\1</span>" )
                            .arg( GlobalApplet::textColorDelayed().name() ) );
    } else {
        sTime.clear();
    }

    return sTime;
}

QList< QVariant > JourneyInfo::vehicleTypesVariant() const
{
    QList<QVariant> vehicleTypesVariant;
    foreach( VehicleType vehicleType, m_vehicleTypes ) {
        vehicleTypesVariant.append( static_cast<int>( vehicleType ) );
    }
    return vehicleTypesVariant;
}

QString JourneyInfo::durationToDepartureString( bool toArrival ) const
{
    int totalSeconds = QDateTime::currentDateTime().secsTo( toArrival ? m_arrival : m_departure );
    int totalMinutes = qCeil(( qreal )totalSeconds / 60.0 );
    if ( totalMinutes < 0 ) {
        return i18nc( "@info/plain", "already left" );
    } else {
        return KGlobal::locale()->prettyFormatDuration( (ulong)(totalMinutes * 60000) );
    }
}

QString DepartureInfo::delayString() const
{
    return delayType() == Delayed ? QString( " (+%1)" ).arg( m_delay ) : QString();
}

QString DepartureInfo::durationString( bool showDelay ) const
{
    int totalSeconds = QDateTime::currentDateTime().secsTo( predictedDeparture() );
    int totalMinutes = qCeil(( qreal )totalSeconds / 60.0 );
    if ( totalMinutes < 0 ) {
        return i18nc( "@info/plain", "already left" );
    }

    QString sDuration;
    if ( totalMinutes == 0 ) {
        sDuration = i18nc( "@info/plain", "now" );
    } else {
        sDuration = KGlobal::locale()->prettyFormatDuration( totalMinutes * 60000 );
    }

    return showDelay ? sDuration + delayString() : sDuration;
}

void DepartureInfo::init( const QString &operatorName, const QString &line,
                        const QString &target, const QDateTime &departure,
                        VehicleType lineType, LineServices lineServices,
                        const QString &platform, int delay,
                        const QString &delayReason, const QString &journeyNews,
                        const QStringList &routeStops, const QList<QTime> &routeTimes,
                        int routeExactStops )
{
    m_filteredOut = false;

    QRegExp rx( "[0-9]*$" );
    rx.indexIn( line );
    if ( rx.isValid() ) {
        m_lineNumber = rx.cap().toInt();
    } else {
        m_lineNumber = 0;
    }

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

bool DepartureInfo::operator ==( const DepartureInfo& other ) const
{
    return m_hash == other.m_hash
// 	These are already in m_hash
// 	&& m_departure == other.m_departure
// 	&& m_vehicleType == other.m_vehicleType
// 	&& m_lineString == other.m_lineString
            && m_lineNumber == other.m_lineNumber
            && m_target == other.m_target
            && m_delay == other.m_delay
            && m_platform == other.m_platform
            && m_delayReason == other.m_delayReason
            && m_operator == other.m_operator
            && m_journeyNews == other.m_journeyNews
            && m_lineServices == other.m_lineServices
            && m_routeStops == other.m_routeStops
            && m_routeTimes == other.m_routeTimes
            && m_routeExactStops == other.m_routeExactStops;
}

void DepartureInfo::generateHash()
{
    m_hash = qHash( QString("%1%2%3%4")
                    .arg(m_departure.toString("dMyyhhmm"))
                    .arg(static_cast<int>(m_vehicleType))
                    .arg(m_lineString)
                    .arg(m_target.trimmed().toLower()) );
}

void JourneyInfo::generateHash()
{
    QString vehicles;
    foreach( int vehicleType, m_vehicleTypes ) {
        vehicles += QString::number( static_cast<int>( vehicleType ) );
    }
    m_hash = qHash( QString("%1%2%3%4")
                    .arg(m_departure.toString("dMyyhhmm"))
                    .arg(m_duration)
                    .arg(m_changes)
                    .arg(vehicles) );
}

QString DepartureInfo::formatDateFancyFuture( const QDate& date )
{
    int dayDiff = QDate::currentDate().daysTo( date );
    if ( dayDiff == 1 ) {
        return i18nc( "@info/plain Used for fancy formatted dates in the future.", "tomorrow" );
    } else if ( dayDiff <= 6 ) {
        return date.toString( "ddd" );
    } else {
        return KGlobal::locale()->formatDate( date, KLocale::ShortDate );
    }
}

QString DepartureInfo::delayText() const
{
    QString sText;
    DelayType type = delayType();
    if ( type == OnSchedule ) {
        sText = i18nc( "@info/plain A public transport vehicle departs on schedule", "On schedule" );
        sText = sText.prepend( QString( "<span style='color:%1;'>" )
                                .arg( GlobalApplet::textColorOnSchedule().name() ) ).append( "</span>" );
    } else if ( type == Delayed ) {
        sText = i18ncp( "@info/plain", "+%1 minute", "+%1 minutes", delay() );
        sText = sText.replace( QRegExp( "(+?\\s*\\d+)" ),
                                QString( "<span style='color:%1;'>+&nbsp;\\1</span>" )
                                .arg( GlobalApplet::textColorDelayed().name() ) );

        if ( !delayReason().isEmpty() ) {
            sText += ", " + delayReason();
        }
    } else { // DelayUnknown:
        sText = i18nc( "@info/plain", "No information available" );
    }

    return sText;
}

QString DepartureInfo::departureText( bool htmlFormatted, bool displayTimeBold,
                                    bool showRemainingMinutes,
                                    bool showDepartureTime, int linesPerRow ) const
{
    QDateTime predictedDep = predictedDeparture();
    QString sTime, sDeparture = predictedDep.toString( "hh:mm" );
    QString sColor;
    if ( htmlFormatted ) {
        if ( delayType() == OnSchedule ) {
            sColor = QString( "color:%1;" ).arg( GlobalApplet::textColorOnSchedule().name() );
        } else if ( delayType() == Delayed ) {
            sColor = QString( "color:%1;" ).arg( GlobalApplet::textColorDelayed().name() );
        }

        QString sBold;
        if ( displayTimeBold ) {
            sBold = "font-weight:bold;";
        }

        sDeparture = sDeparture.prepend( QString("<span style='%1%2'>").arg(sColor).arg(sBold) )
                            .append( "</span>" );
    }
    if ( predictedDeparture().date() != QDate::currentDate() ) {
        sDeparture += ", " + formatDateFancyFuture( predictedDep.date() );
    }

    if ( showDepartureTime && showRemainingMinutes ) {
        QString sText = durationString( false );
        sDeparture += delayString(); // Show delay after time
        if ( htmlFormatted ) {
            sDeparture = sDeparture.replace( QRegExp( "(\\(?\\+(?:\\s|&nbsp;)*\\d+\\)?)" ),
                                            QString( "<span style='color:%1;'>\\1</span>" )
                                            .arg( GlobalApplet::textColorDelayed().name() ) );
            sTime = QString( linesPerRow > 1 ? "%1<br>(%2)" : "%1 (%2)" )
                    .arg( sDeparture ).arg( sText );
        } else {
            sTime = QString( linesPerRow > 1 ? "%1\n(%2)" : "%1 (%2)" )
                    .arg( sDeparture ).arg( sText );
        }
    } else if ( showDepartureTime ) {
        if ( htmlFormatted ) {
            sDeparture += delayString().replace( QRegExp( "(\\(?\\+(?:\\s|&nbsp;)*\\d+\\)?)" ),
                            QString( "<span style='color:%1;'>\\1</span>" )
                            .arg( GlobalApplet::textColorDelayed().name() ) ); // Show delay after time
        } else {
            sDeparture += delayString();
        }
        sTime = sDeparture;
    } else if ( showRemainingMinutes ) {
        sTime = durationString();
        if ( htmlFormatted ) {
            if ( linesPerRow == 1 ) {
                sTime = sTime.replace( ' ', "&nbsp;" ); // No line breaking
            }
            DelayType type = delayType();
            if ( type == Delayed ) {
                sTime = sTime.replace( QRegExp( "(\\(?\\+(?:\\s|&nbsp;)*\\d+\\)?)" ),
                                    QString( "<span style='color:%1;'>\\1</span>" )
                                    .arg( GlobalApplet::textColorDelayed().name() ) );
            } else if ( type == OnSchedule ) {
                sTime = sTime.prepend( QString( "<span style='color:%1;'>" )
                                    .arg( GlobalApplet::textColorOnSchedule().name() ) )
                        .append( "</span>" );
            }
        }
    } else {
        sTime.clear();
    }

    return sTime;
}

uint qHash( const DepartureInfo& departureInfo )
{
    return departureInfo.hash();
}

bool operator <( const JourneyInfo& ji1, const JourneyInfo& ji2 )
{
    return ji1.departure() < ji2.departure();
}

bool operator <( const DepartureInfo &di1, const DepartureInfo &di2 )
{
    return di1.predictedDeparture() < di2.predictedDeparture();
}

QDebug& operator<<( QDebug debug, const DepartureInfo& departureInfo )
{
    return debug << QString( "(%1 %2 at %3)" )
            .arg( departureInfo.lineString() )
            .arg( departureInfo.target() )
            .arg( departureInfo.predictedDeparture().toString() );
}

QDebug& operator<<( QDebug debug, const JourneyInfo& journeyInfo )
{
    return debug << QString( "(from %1 to %2, %3, %4 changes at %5)" )
            .arg( journeyInfo.startStopName() )
            .arg( journeyInfo.targetStopName() )
            .arg( journeyInfo.durationToDepartureString() )
            .arg( journeyInfo.changes() )
            .arg( journeyInfo.departure().toString() );
}
