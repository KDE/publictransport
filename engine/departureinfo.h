/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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

/** @file
 * @brief This file contains classes to store departure / arrival / journey information.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef DEPARTUREINFO_HEADER
#define DEPARTUREINFO_HEADER

// Own includes
#include "enums.h"

// Qt includes
#include <QVariant>
#include <QTime>
#include <QStringList>
#include <QPointer>

/**
 * @brief LineService-Flags.
 * @see LineService
 **/
Q_DECLARE_FLAGS( LineServices, Enums::LineService )

/**
 * @brief This is the abstract base class of all other timetable information classes.
 *
 * @see JourneyInfo
 * @see DepartureInfo
 **/
class PublicTransportInfo : public QObject {
public:
    /** @brief Options for stop names, eg. use a shortened form or not. */
    enum StopNameOptions {
        UseFullStopNames, /**< Use the full stop names, as received from the service provider. */
        UseShortenedStopNames /**< Use a shortened form of the stop names. */
    };

    /** @brief Constructs a new PublicTransportInfo object. */
    PublicTransportInfo( QObject *parent = 0 );

    enum Correction {
        NoCorrection                    = 0x0000,
        DeduceMissingValues             = 0x0001,
        TestValuesForCorrectFormat      = 0x0002,
        CombineToPreferredValueType     = 0x0004, /**< eg. combine DepartureHour and
                * DepartureMinute into the preferred value type DepartureTime.
                * TODO Update TimetableInformation enum documentation. */
        CorrectEverything = DeduceMissingValues | TestValuesForCorrectFormat |
                CombineToPreferredValueType
    };
    Q_DECLARE_FLAGS( Corrections, Correction );

    /**
     * @brief Contructs a new PublicTransportInfo object based on the information given
     *   with @p data.
     *
     * @param data A hash that contains values for TimetableInformations.
     **/
    explicit PublicTransportInfo( const TimetableData &data,
                                  Corrections corrections = CorrectEverything,
                                  QObject *parent = 0 );

    virtual ~PublicTransportInfo();

    bool contains( Enums::TimetableInformation info ) const { return m_data.contains(info); };
    QVariant value( Enums::TimetableInformation info ) const { return m_data[info]; };
    void insert( Enums::TimetableInformation info, const QVariant &data ) {
        m_data.insert( info, data );
    };
    void remove( Enums::TimetableInformation info ) { m_data.remove(info); };

    /** @brief Returns the TimetableData object for this item. */
    TimetableData data() const { return m_data; };

    /**
     * @brief Wheather or not this PublicTransportInfo object is valid.
     *
     * @return true if the PublicTransportInfo object is valid.
     * @return false if the PublicTransportInfo object is invalid.
     **/
    virtual bool isValid() const { return m_isValid; };

protected:
    bool m_isValid;
    TimetableData m_data;
};

/**
 * @brief This class stores information about journeys with public transport.
 *
 * @see DepartureInfo
 * @see PublicTransportInfo
 */
class JourneyInfo : public PublicTransportInfo {
public:
    /**
     * @brief Contructs a new JourneyInfo object based on the information given with @p data.
     *
     * @param data A hash that contains values for at least the required
     *   TimetableInformations: DepartureDateTime, ArrivalDateTime, StartStopName and
     *   TargetStopName. Instead of DepartureDateTime, DepartureDate and DepartureTime can be used.
     *   If only DepartureTime gets used, the date is guessed. The same is true for ArrivalDateTime.
     **/
    explicit JourneyInfo( const TimetableData &data, Corrections corrections = CorrectEverything,
                          QObject *parent = 0 );

    QStringList vehicleIconNames() const;
    QStringList vehicleNames( bool plural = false ) const;
};

/**
 * @brief This class stores information about departures / arrivals with public transport.
 *
 * @see JourneyInfo
 * @see PublicTransportInfo
 */
class DepartureInfo : public PublicTransportInfo {
public:
    /** @brief Constructs an invalid DepartureInfo object. */
    DepartureInfo( QObject *parent = 0 );

    /**
     * @brief Contructs a new DepartureInfo object based on the information given with @p data.
     *
     * @param data A hash that contains values for at least the required
     *   TimetableInformations: TransportLine, Target, DepartureDateTime. Instead of
     *   DepartureDateTime, DepartureDate and DepartureTime can be used. If only DepartureTime
     *   gets used, the date is guessed.
     **/
    explicit DepartureInfo( const TimetableData &data, Corrections corrections = CorrectEverything,
                            QObject *parent = 0 );

    /** @brief Wheather or not the departing / arriving vehicle is a night line. */
    bool isNightLine() const { return m_lineServices.testFlag( Enums::NightLine ); };

    /** @brief Wheather or not the departing / arriving vehicle is an express line. */
    bool isExpressLine() const { return m_lineServices.testFlag( Enums::ExpressLine ); };

private:
    LineServices m_lineServices;
};

/**
 * @brief Stores information about a stop. Used for stop suggestions.
 *
 * @see DepartureInfo
 * @see JourneyInfo
 **/
class StopInfo : public PublicTransportInfo {
public:
    /** @brief Constructs an invalid StopInfo object. */
    StopInfo( QObject *parent = 0 );

    /**
     * @brief Contructs a new StopInfo object based on the information given with @p data.
     *
     * @param data A hash that contains values for at least the required TimetableInformations
     *   (StopName). */
    StopInfo( const QHash<Enums::TimetableInformation, QVariant> &data, QObject *parent = 0 );

    /**
     * @brief Constructs a new StopInfo object.
     *
     * @param name The name of the stop.
     * @param id The ID for the stop @p name, if available.
     * @param weight The weight of this stop suggestion, if available. Higher values are set for
     *   more important / better matching stops.
     * @param city The city in which the stop is, if available.
     * @param countryCode The code of the country in which the stop is, if available.
     */
    StopInfo( const QString &name, const QString &id = QString(), int weight = -1,
              const QString &city = QString(), const QString &countryCode = QString(),
              QObject *parent = 0 );
};

typedef DepartureInfo ArrivalInfo;
typedef QSharedPointer< PublicTransportInfo > PublicTransportInfoPtr;
typedef QSharedPointer< DepartureInfo > DepartureInfoPtr;
typedef QSharedPointer< ArrivalInfo > ArrivalInfoPtr;
typedef QSharedPointer< JourneyInfo > JourneyInfoPtr;
typedef QSharedPointer< StopInfo > StopInfoPtr;
typedef QList< PublicTransportInfoPtr > PublicTransportInfoList;
typedef QList< DepartureInfoPtr > DepartureInfoList;
typedef QList< ArrivalInfoPtr > ArrivalInfoList;
typedef QList< JourneyInfoPtr > JourneyInfoList;
typedef QList< StopInfoPtr > StopInfoList;

#endif // DEPARTUREINFO_HEADER
