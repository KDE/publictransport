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

// Own includes
#include "stoplineedit.h"
#include "stopsettings.h"

// Plasma/KDE includes
#include <Plasma/DataEngineManager>
#include <Plasma/ServiceJob>
#include <KColorScheme>
#include <KIcon>

// Qt includes
#include <QEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

// Private class of StopLineEdit
class StopLineEditPrivate
{
	Q_DECLARE_PUBLIC( StopLineEdit )

public:
    enum State {
        Ready,
        WaitingForDataEngineProgress,
        WaitingForStopSuggestions,
        Error
    };

	StopLineEditPrivate( const QString &serviceProvider, StopLineEdit *q ) : q_ptr( q ) {
        state = Ready;
        progress = 0.0;

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
    State state;
    qreal progress; // Progress of the data engine in processing a task (0.0 .. 1.0)
    QString sourceName; // Source name used to request stop suggestions at the data engine
    QString errorString;
	
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
    d->state = StopLineEditPrivate::Ready;
    if ( !d->sourceName.isEmpty() ) {
        d->publicTransportEngine->disconnectSource( d->sourceName, this );
    }
    setEnabled( true );
    setClearButtonShown( true ); // Stays enabled and does not get drawn in KLineEdit::paintEvent
    setReadOnly( false );
    completionObject()->clear();
    edited( text() );
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
    d->state = StopLineEditPrivate::Ready;
    if ( !d->sourceName.isEmpty() ) {
        d->publicTransportEngine->disconnectSource( d->sourceName, this );
    }
    setEnabled( true );
    setClearButtonShown( true ); // Stays enabled and does not get drawn in KLineEdit::paintEvent
    setReadOnly( false );
    completionObject()->clear();
    edited( text() );
}

QString StopLineEdit::city() const
{
	Q_D( const StopLineEdit );
	return d->city;
}

void StopLineEdit::edited( const QString& newText )
{
	Q_D( StopLineEdit );

    // Do not connect new sources, if the data engine indicated that it is currently processing
    // a task (ie. download a GTFS feed or import it into the database)
    if ( d->state == StopLineEditPrivate::WaitingForDataEngineProgress ) {
        return;
    }
	
	// Don't request new suggestions if newText is one of the suggestions, ie. most likely a
	// suggestion was selected. To allow choosing another suggestion with arrow keys the old
	// suggestions shouldn't be removed in this case and no update is needed.
	foreach ( const Stop &stop, d->stops ) {
		if ( stop.name.compare(newText, Qt::CaseInsensitive) == 0 ) {
			return; // Don't update suggestions
		}
	}

    if ( !d->sourceName.isEmpty() ) {
        d->publicTransportEngine->disconnectSource( d->sourceName, this );
    }

    d->state = StopLineEditPrivate::WaitingForStopSuggestions;
    d->sourceName = QString("Stops %1|stop=%2").arg( d->serviceProvider, newText.isEmpty() ? " " : newText ); // TODO
	if ( !d->city.isEmpty() ) { // m_useSeparateCityValue ) {
		d->sourceName += QString("|city=%3").arg( d->city );
	}
	d->publicTransportEngine->connectSource( d->sourceName, this );
}

void StopLineEdit::paintEvent( QPaintEvent *ev )
{
    Q_D( StopLineEdit );

    if ( d->state == StopLineEditPrivate::WaitingForDataEngineProgress && paintEngine() ) {
        if ( paintEngine() ) {
            QSize cancelButtonSize = clearButtonUsedSize();
            QRect cancelButtonRect( QPoint(contentsRect().right() - cancelButtonSize.width() - 1,
                                        (contentsRect().height() - cancelButtonSize.height()) / 2.0),
                                    cancelButtonSize );

            // Draw a progress bar while waiting for the data engine to complete a task
            QStyleOptionProgressBar option;
            option.initFrom( this );
            option.minimum = 0;
            option.maximum = 100;
            option.progress = d->progress * 100;
            option.text = i18nc( "@info/plain", "Loading GTFS feed... %1 %", d->progress * 100 );
            option.textAlignment = Qt::AlignCenter;
            option.textVisible = true;
            option.rect.setWidth( option.rect.width() - cancelButtonSize.width() - 1 );

            QPainter painter( this );
            style()->drawControl( QStyle::CE_ProgressBar, &option, &painter );
            painter.drawPixmap( cancelButtonRect, KIcon("dialog-cancel").pixmap(cancelButtonSize) );
        } else { kDebug() << "no paint engine"; }
    } else if ( d->state == StopLineEditPrivate::Error && paintEngine() ) {
        if ( paintEngine() ) {
            const QColor errorColor = KColorScheme( QPalette::Normal, KColorScheme::View )
                    .foreground( KColorScheme::NegativeText ).color();
            QStyleOptionFrameV3 option;
            option.initFrom( this );
            option.frameShape = QFrame::StyledPanel;
            option.state = QStyle::State_Sunken;
            option.features = QStyleOptionFrameV2::None;
            option.lineWidth = 1;
            option.midLineWidth = 1;

            QPainter painter( this );
            style()->drawControl( QStyle::CE_ShapedFrame, &option, &painter );
            painter.setPen( errorColor );

            const QString errorText = d->errorString.isEmpty()
                    ? i18nc("@info/plain", "Error loading the Service Provider") : d->errorString;
            painter.drawText( contentsRect(), Qt::AlignCenter,
                    fontMetrics().elidedText(errorText, Qt::ElideMiddle, option.rect.width() - 4) );
        } else { kDebug() << "no paint engine"; }
    } else {
        KLineEdit::paintEvent( ev );
    }
}

void StopLineEdit::importProgress( KJob *job, ulong percent )
{
    Q_D( StopLineEdit );
    d->progress = percent / 100.0;
    setToolTip( i18nc("@info/plain", "Importing the GTFS feed for stop suggestions. Please wait.") );
}

void StopLineEdit::importFinished( KJob *job )
{
    Q_D( StopLineEdit );

    const bool hasError = job->error() < 0;
    setEnabled( !hasError );
    setClearButtonShown( !hasError ); // Stays enabled and does not get drawn in KLineEdit::paintEvent
    setReadOnly( hasError );
    d->state = hasError ? StopLineEditPrivate::Error : StopLineEditPrivate::Ready;
    d->errorString = job->errorString(); // TODO needed?
    setToolTip( job->errorString() );
}

void StopLineEdit::dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data& data )
{
	Q_D( StopLineEdit );
	
	if ( !sourceName.startsWith(QLatin1String("Stops")) || d->sourceName != sourceName ) {
        kDebug() << "Wrong (old) source" << sourceName;
		return;
	}

	kDebug() << "Updated Data For Source" << sourceName;
	
	// Stop suggestions data
//     kDebug() << data["type"];
	if ( data["type"].toString().compare("GTFS", Qt::CaseInsensitive) == 0 && // TODO
         data["error"].toBool() )
    {
        d->state = StopLineEditPrivate::WaitingForDataEngineProgress;
        setEnabled( false );
        setClearButtonShown( false ); // Stays enabled and does not get drawn in KLineEdit::paintEvent
        setReadOnly( true );

        kDebug() << "GTFS accessor with an error, use service to update using the GTFS feed";
        Plasma::Service *service = d->publicTransportEngine->serviceForSource( QString() );
        KConfigGroup op = service->operationDescription("updateGtfsFeed");
        op.writeEntry( "serviceProviderId", d->serviceProvider );
        Plasma::ServiceJob *job = service->startOperationCall( op );
        connect( job, SIGNAL(finished(KJob*)), this, SLOT(importFinished(KJob*)) );
        connect( job, SIGNAL(percent(KJob*,ulong)), this, SLOT(importProgress(KJob*,ulong)) );
        return;
    }

	d->progress = data.contains("progress") ? data.value("progress").toReal() : -1.0;
    if ( d->progress >= 0.0 && d->progress < 1.0 ) {
        if ( d->state != StopLineEditPrivate::WaitingForDataEngineProgress ) {
            d->state = StopLineEditPrivate::WaitingForDataEngineProgress;
            setEnabled( false );
            setClearButtonShown( false ); // Stays enabled and does not get drawn in KLineEdit::paintEvent
            setReadOnly( true );
        }

        // Update the progress bar
        update();
        return;
    } else if ( qFuzzyCompare(d->progress, 1.0) || !isEnabled() ||
                d->state == StopLineEditPrivate::WaitingForDataEngineProgress )
    {
        // The data engine just completed a task
        d->progress = 0.0;
    }

    if ( !d->sourceName.isEmpty() ) {
        d->publicTransportEngine->disconnectSource( d->sourceName, this );
        d->sourceName.clear();
    }

    if ( data.value("error").toBool() ) {
		kDebug() << "Stop suggestions error" << sourceName;
        d->state = StopLineEditPrivate::Error;
	} else if ( !data.value("receivedPossibleStopList").toBool() ) {
		kDebug() << "No stop suggestions received" << sourceName;
        d->state = StopLineEditPrivate::Error;
	} else {
        d->state = StopLineEditPrivate::Ready;
    }

    // Enable if no error / disable otherwise
    setEnabled( d->state != StopLineEditPrivate::Error );
    setClearButtonShown( isEnabled() ); // Stays enabled and does not get drawn in KLineEdit::paintEvent
    setReadOnly( !isEnabled() );
    if ( d->state == StopLineEditPrivate::Error ) {
        return;
    }
	
	d->stops.clear();
	
 	QStringList weightedStops;
	QHash<Stop, QVariant> stopToStopWeight;
	int count = data["count"].toInt();
	for ( int i = 0; i < count; ++i ) {
		const QVariant stopData = data.value( QString("stopName %1").arg(i) );
		if ( !stopData.isValid() ) {
			continue;
		}

		const QHash<QString, QVariant> dataMap = stopData.toHash();
		const QString stopName = dataMap["stopName"].toString();
		const QString stopID = dataMap["stopID"].toString();
		const int stopWeight = dataMap["stopWeight"].toInt();
		
		const Stop stop( stopName, stopID );
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

	// Only add stop suggestions, if the line edit still has focus
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
