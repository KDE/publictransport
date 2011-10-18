/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

#include "stoplineedit.h"
#include "stopsettings.h"

#include <Plasma/DataEngineManager>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

// Private class of StopLineEdit
class StopLineEditPrivate
{
    Q_DECLARE_PUBLIC( StopLineEdit )

public:
    StopLineEditPrivate( const QString &serviceProvider, StopLineEdit *q ) : q_ptr( q ) {
        // Load data engine
        dataEngineManager = Plasma::DataEngineManager::self();
        publicTransportEngine = dataEngineManager->loadEngine("publictransport");

        this->serviceProvider = serviceProvider;
    };

    ~StopLineEditPrivate() {
        if ( dataEngineManager ) {
            dataEngineManager->unloadEngine("publictransport");
        }
    }

    Plasma::DataEngineManager *dataEngineManager;
    Plasma::DataEngine *publicTransportEngine;
    StopList stops;
    QString city;
    QString serviceProvider;

protected:
    StopLineEdit* const q_ptr;
};

StopLineEdit::StopLineEdit( QWidget* parent, const QString &serviceProvider,
                            KGlobalSettings::Completion completion )
        : KLineEdit( parent ), d_ptr( new StopLineEditPrivate(serviceProvider, this) )
{
    setCompletionMode( completion );
    connect( this, SIGNAL(textEdited(QString)), this, SLOT(edited(QString)) );
}

StopLineEdit::~StopLineEdit()
{
    delete d_ptr;
}

void StopLineEdit::setServiceProvider( const QString& serviceProvider )
{
    Q_D( StopLineEdit );
    d->serviceProvider = serviceProvider;
}

QString StopLineEdit::serviceProvider() const
{
    Q_D( const StopLineEdit );
    return d->serviceProvider;
}

void StopLineEdit::setCity( const QString& city )
{
    Q_D( StopLineEdit );
    d->city = city;
}

QString StopLineEdit::city() const
{
    Q_D( const StopLineEdit );
    return d->city;
}

void StopLineEdit::edited( const QString& newText )
{
    Q_D( StopLineEdit );

    // Don't request new suggestions if newText is one of the suggestions, ie. most likely a
    // suggestion was selected. To allow choosing another suggestion with arrow keys the old
    // suggestions shouldn't be removed in this case and no update is needed.
//     bool suggestionSelected = false;
    foreach ( const Stop &stop, d->stops ) {
        if ( stop.name.compare(newText, Qt::CaseInsensitive) == 0 ) {
            return; // Don't update suggestions
        }
    }

    if ( !d->city.isEmpty() ) { // m_useSeparateCityValue ) {
        d->publicTransportEngine->connectSource( QString("Stops %1|stop=%2|city=%3")
                .arg(d->serviceProvider, newText, d->city), this );
    } else {
        d->publicTransportEngine->connectSource( QString("Stops %1|stop=%2")
                .arg(d->serviceProvider, newText), this );
    }
}

void StopLineEdit::dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data& data )
{
    Q_D( StopLineEdit );

    if ( !sourceName.startsWith(QLatin1String("Stops")) ) {
        return;
    }

    // Stop suggestions data
    if ( data.value("error").toBool() ) {
        kDebug() << "Stop suggestions error" << sourceName;
        // TODO: Handle error somehow?
        return;
    } else if ( !data.value("receivedPossibleStopList").toBool() ) {
        kDebug() << "No stop suggestions received" << sourceName;
        return;
    }

    d->stops.clear();

     QStringList weightedStops;
    QHash<Stop, QVariant> stopToStopWeight;
    int count = data["count"].toInt();
    for ( int i = 0; i < count; ++i ) {
        QVariant stopData = data.value( QString("stopName %1").arg(i) );
        if ( !stopData.isValid() ) {
            continue;
        }

        QHash<QString, QVariant> dataMap = stopData.toHash();
        QString sStopName = dataMap["stopName"].toString();
        QString sStopID = dataMap["stopID"].toString();
        int stopWeight = dataMap["stopWeight"].toInt();

        Stop stop( sStopName, sStopID );
        stopToStopWeight.insert( stop, stopWeight );
        d->stops << stop;
    }

    // Construct weighted stop list for KCompletion
    bool hasAtLeastOneWeight = false;
    foreach ( const Stop &stop, d->stops ) {
        int stopWeight = stopToStopWeight[ stop ].toInt();
        if ( stopWeight <= 0 ) {
            stopWeight = 0;
        } else {
            hasAtLeastOneWeight = true;
        }

        weightedStops << QString( "%1:%2" ).arg( stop ).arg( stopWeight );
    }

//     KLineEdit *stop = stopList->focusedLineEdit();
    if ( hasFocus() ) {
        kDebug() << "Prepare completion object";
        KCompletion *comp = completionObject();
        comp->setIgnoreCase( true );
        if ( hasAtLeastOneWeight ) {
            comp->setOrder( KCompletion::Weighted );
            comp->insertItems( weightedStops );
        } else {
            comp->setOrder( KCompletion::Insertion );
            QStringList stopList;
            foreach ( const Stop &stop, d->stops ) {
                stopList << stop.name;
            }
            comp->insertItems( stopList );
        }

        // Complete manually, because the completions are requested asynchronously
        doCompletion( text() );
    } else {
        kDebug() << "The stop line edit doesn't have focus, discard received stops.";
    }
}

StopLineEditList::StopLineEditList( QWidget* parent,
        AbstractDynamicWidgetContainer::RemoveButtonOptions removeButtonOptions,
        AbstractDynamicWidgetContainer::AddButtonOptions addButtonOptions,
        AbstractDynamicWidgetContainer::SeparatorOptions separatorOptions,
        AbstractDynamicWidgetContainer::NewWidgetPosition newWidgetPosition,
        const QString& labelText )
        : DynamicLabeledLineEditList( parent, removeButtonOptions, addButtonOptions,
                                      separatorOptions, newWidgetPosition, labelText )
{

}

KLineEdit* StopLineEditList::createLineEdit()
{
    return new StopLineEdit( this );
}

void StopLineEditList::setCity(const QString& city)
{
    foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
        dynamicWidget->contentWidget<StopLineEdit*>()->setCity( city );
    }
}

void StopLineEditList::setServiceProvider(const QString& serviceProvider)
{
    foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
        dynamicWidget->contentWidget<StopLineEdit*>()->setServiceProvider( serviceProvider );
    }
}

}; // namespace Timetable
