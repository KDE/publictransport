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

#ifndef PUBLICTRANSPORTRUNNER_H
#define PUBLICTRANSPORTRUNNER_H

#include <plasma/abstractrunner.h>
#include <KIcon>
#include <Plasma/DataEngine>
#include "config/publictransportrunner_config.h"

class QSemaphore;
class MarbleProcess;
class PublicTransportRunnerHelper;

// Define our plasma Runner
class PublicTransportRunner : public Plasma::AbstractRunner {
    Q_OBJECT

public:
    /**
     * @brief Used to identify the given keywords.
     **/
    enum Keyword {
        NoKeyword           = 0x0000,

        // Main keywords
        Journeys            = 0x0001,
        Departures          = 0x0002,
        Arrivals            = 0x0004,
        StopSuggestions     = 0x0008,

        // Filter keywords
        OnlyPublicTransport = 0x0010,
        OnlyBuses           = 0x0020,
        OnlyTrams           = 0x0040,
        OnlyTrains          = 0x0080
    };
    Q_DECLARE_FLAGS( Keywords, Keyword )

    struct QueryData {
        Keywords keywords;
        int minutesUntilFirstResult;
    };

    struct Settings {
        QString location;
        QString serviceProviderID;
        QString city;
        QString keywordDeparture;
        QString keywordArrival;
        QString keywordJourney;
        QString keywordStop;
        int resultCount;
    };

    // Basic Create/Destroy
    PublicTransportRunner( QObject *parent, const QVariantList& args );
    ~PublicTransportRunner();

    void match(Plasma::RunnerContext &context);
    void run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match);

    virtual void reloadConfiguration();
    Settings settings() { return m_settings; };

    QMutex &mutex() { return m_mutex; };

signals:
    void doMatch(PublicTransportRunner *runner, Plasma::DataEngine *engine, Plasma::RunnerContext *context);

protected slots:
    void init();
    void marbleFinished();

private:
    QMutex m_mutex;
    PublicTransportRunnerHelper *m_helper;
    Settings m_settings;
    QSemaphore *m_semaphore;
    MarbleProcess *m_marble;
};
// This is the command that links the runner to the .desktop file
K_EXPORT_PLASMA_RUNNER(publictransport, PublicTransportRunner)
Q_DECLARE_OPERATORS_FOR_FLAGS( PublicTransportRunner::Keywords )


class PublicTransportRunnerHelper : public QObject {
    Q_OBJECT

public:
    enum FinishType {
        FinishedSuccessfully = 0,
        FinishedWithErrors,
        Aborted
    };

    explicit PublicTransportRunnerHelper( PublicTransportRunner *runner );

signals:
    void matchFinished( FinishType finishType = FinishedSuccessfully );

public slots:
    void match( PublicTransportRunner *runner, Plasma::DataEngine *engine,
                Plasma::RunnerContext *context );
};

/**
* @brief Contains information about a single match from the search.
*/
class Result : public QObject {
    Q_OBJECT

public:
    Result() : QObject(0) {
    };

    Result( const Result &other ) : QObject(0) {
        this->text = other.text;
        this->url = other.url;
        this->icon = other.icon;
        this->subtext = other.subtext;
        this->relevance = other.relevance;
        this->data = other.data;
    };

    Result &operator =( const Result &other ) {
        this->text = other.text;
        this->url = other.url;
        this->icon = other.icon;
        this->subtext = other.subtext;
        this->relevance = other.relevance;
        this->data = other.data;

        return *this;
    };

    /** @brief The main text to show for this timetable result. */
    QString text;

    /** @brief Additional text to show for this timetable result. */
    QString subtext;

    /** @brief An icon showing the used vehicle type(s) for this timetable result. */
    KIcon icon;

    /** @brief The URL of the page containing the match, ie. the service provider. */
    QUrl url;

    /** @brief The relevance of this result, computed from sort by using @ref normalizeRelevance. */
    qreal relevance;

    /** @brief Additional data to store for the result. */
    QVariantHash data;
};
Q_DECLARE_METATYPE( Result )

class AsyncDataEngineUpdater : public QObject {
    Q_OBJECT

public:

    AsyncDataEngineUpdater( Plasma::DataEngine *engine, Plasma::RunnerContext *context,
                            PublicTransportRunner *runner );
    virtual ~AsyncDataEngineUpdater() {};

    QList<Result> results() const;
    Plasma::DataEngine *engine() const;

signals:
    /**
     * @brief Emitted when a search has been completed.
     *
     * @param success true if the search was completed successfully.
     **/
    void finished( bool success );

public slots:
    void query( Plasma::DataEngine *engine, const PublicTransportRunner::QueryData &data,
                const QString &stop, const QString &stop2 = QString() );

    /** @brief Aborts the currently running request. */
    void abort();

protected slots:
    /** @brief New data arrived from the publicTransport data engine. */
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

private:
    /** @brief Read stop suggestions from the publicTransport data engine. */
    void processStopSuggestions(const QString &sourceName, const Plasma::DataEngine::Data& data );

    void processDepartures(const QString &sourceName, const Plasma::DataEngine::Data& data );

    void processJourneys(const QString &sourceName, const Plasma::DataEngine::Data &data );

    static bool isTimeShown( const QDateTime& dateTime, int timeOffsetOfFirstDeparture );

    void normalizeRelevance( qreal min, qreal max );

    QList<Result> m_results;
    Plasma::DataEngine *m_engine;
    QPointer< Plasma::RunnerContext > m_context;
    PublicTransportRunner::QueryData m_data;
    QString m_sourceName;
    PublicTransportRunner::Settings m_settings;
    PublicTransportRunner *m_runner;
};

#endif
