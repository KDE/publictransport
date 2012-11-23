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

// Own includes
#include "stoplineedit.h"
#include "stopsettings.h"

#ifdef MARBLE_FOUND
    #include "publictransportmapwidget.h"
    #include "publictransportlayer.h"
    #include "popuphandler.h"
#endif

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
#include <QMouseEvent>
#include <QTimer>
#include <qmath.h>

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

/// Private class of StopLineEdit
class StopLineEditPrivate
{
    Q_DECLARE_PUBLIC( StopLineEdit )

public:
    /// Different states of the StopLineEdit
    enum State {
        Ready, ///< Ready to get stop suggestions
        WaitingForImport, ///< Waiting for the first import (eg. of the associated GTFS feed).
                ///< Later imports are updates and can be done in the background. This StopLineEdit
                ///< gets put into read only mode and the progress gets visualized by a progress bar.
                ///< An additional cancel button is shown to cancel the import.
        WaitingForStopSuggestions, ///< Waiting for an answer from the accessor with stop
                ///< suggestions. This StopLineEdit is in read write mode and is enabled.
                ///< Completions are added when the stop suggestions arrive.
        AskingToImportGtfsFeed, ///< Currently showing a question whether or not to import the
                ///< GTFS feed. This StopLineEdit gets put into read only mode, but is enabled
                ///< to be able to receive events for additional buttons (in this case these are the
                ///< answer buttons).
        Pause, ///< The import is not finished but suspended.
        Error ///< There has been an error in the data engine / service. The error gets shown and
                ///< this StopLineEdit gets put into read only mode.
    };

    /// Different button types for additional buttons
    enum ButtonType {
        StartButton  = 0x01,
        StopButton   = 0x02,
        PauseButton  = 0x04
    };
    Q_DECLARE_FLAGS( ButtonTypes, ButtonType );

    /// Different states for an additional button
    enum ButtonState {
        ButtonNormal  = 0x00, ///< Normal state (not pressed or hovered)
        ButtonPressed = 0x01, ///< Button is currently pressed
        ButtonPressedMouseOutside = 0x02, ///< The mouse button was pressed on the button but
                ///< then moved outside without releasing the mouse button
        ButtonHovered = 0x04 ///< The mouse is currently over the button. If another button
                ///< currently has state ButtonPressedMouseOutside only that button may get hovered.
    };
    Q_DECLARE_FLAGS( ButtonStates, ButtonState );

    /// Provides information about an additional button to be shown inside the StopLineEdit
    struct Button {
        ButtonType type;
        ButtonStates state;
        QRect rect;
        KIcon icon;
        QString toolTip;

        Button( ButtonType type, const KIcon &icon, const QRect &rect,
                const QString &tooltip = QString() )
                : type(type), state(ButtonNormal), rect(rect), icon(icon), toolTip(tooltip)
        {
        };
    };

    StopLineEditPrivate( const QString &serviceProvider, StopLineEdit *q )
            : importJob(0), q_ptr(q)
    {
        state = Ready;
        progress = 0;

        // Load data engine
        Plasma::DataEngineManager::self()->loadEngine("publictransport");

        this->serviceProvider = serviceProvider;
    };

    ~StopLineEditPrivate()
    {
#ifdef MARBLE_FOUND
        delete mapPopup;
#endif
        Plasma::DataEngineManager::self()->unloadEngine("publictransport");
    };

    /// Get the additional button at the given @p point. The index of the button gets stored in
    /// @p index, if it is not null.
    Button *buttonAt( const QPoint &point, int *index = 0 )
    {
        for ( int i = 0; i < buttons.count(); ++i ) {
            if ( buttons[i].rect.contains(point) ) {
                if ( index ) {
                    *index = i;
                }
                return &buttons[i];
            }
        }

        if ( index ) {
            *index = -1;
        }
        return 0;
    };

    /// Get the additional button that is currently hovered or 0, if no button is hovered.
    /// The index of the button gets stored in @p index, if it is not null.
    Button *hoveredButton( int *index = 0 )
    {
        for ( int i = 0; i < buttons.count(); ++i ) {
            if ( buttons[i].state.testFlag(ButtonHovered) ) {
                if ( index ) {
                    *index = i;
                }
                return &buttons[i];
            }
        }

        if ( index ) {
            *index = -1;
        }
        return 0;
    };

    /// Add the buttons set in @p newButtons.
    /// @note Before adding the buttons all previous buttons are removed.
    void addButtons( ButtonTypes newButtons )
    {
        // Buttons get placed into the StopLineEdit from right to left.
        // That means that the first button added to buttons, gets placed at the right.
        buttons.clear();
        if ( newButtons.testFlag(StopButton) ) {
            Button stopButton( StopButton, KIcon("media-playback-stop"), QRect(),
                    i18nc("@info:tooltip", "<title>Stop import</title>"
                          "<para>Click to cancel the currently running import of the GTFS feed. "
                          "You won't be able to use the service provider without completing the "
                          "import. The import can be started again later, but it will start at "
                          "the beginning.</para>") );
            buttons << stopButton;
        }
        if ( newButtons.testFlag(PauseButton) ) {
            Button pauseButton( PauseButton, KIcon("media-playback-pause"), QRect(),
                    i18nc("@info:tooltip", "<title>Pause/Continue import</title>"
                          "<para>Click to interrupt import. Click again to continue.</para>") );
            buttons << pauseButton;
        }
        if ( newButtons.testFlag(StartButton) ) {
            Button startButton( StartButton, KIcon("media-playback-start"), QRect(),
                    i18nc("@info:tooltip", "<title>Start GTFS feed import</title>"
                          "<para>Click here to download the GTFS feed and import it into the "
                          "database.</para>") );
            buttons << startButton;
        }
    };

    /// Called on resize to update the positions of the additional buttons.
    void updateButtonRects( const QRect &contentsRect )
    {
        const int buttonPadding = 4;
        const QSize buttonSize( 16, 16 );
        const int buttonTop = contentsRect.top() + (contentsRect.height() - buttonSize.height()) / 2;
        int lastButtonLeft = contentsRect.right();
        for ( int i = 0; i < buttons.count(); ++i ) {
            const QRect rect( QPoint(lastButtonLeft - buttonPadding - buttonSize.width(),
                                     buttonTop), buttonSize );
            buttons[i].rect = rect;
            lastButtonLeft = rect.left();
        }
    };

    /// Draws the additional buttons and returns the horizontal space used.
    int drawAdditionalButtons( QPainter *painter ) {
        int buttonsWidth = buttons.isEmpty() ? 0 : 4;
        foreach ( const StopLineEditPrivate::Button &button, buttons ) {
            painter->drawPixmap( button.state.testFlag(StopLineEditPrivate::ButtonPressed)
                                 ? button.rect.adjusted(1, 1, 1, 1) : button.rect,
                                 button.icon.pixmap(button.rect.size(),
                                 button.state.testFlag(StopLineEditPrivate::ButtonHovered)
                                 ? QIcon::Active : QIcon::Normal) );
            buttonsWidth += 4 + button.rect.width();
        }
        return buttonsWidth;
    };

    void connectStopsSource( const QString &stopName ) {
        Q_Q( StopLineEdit );
        Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine("publictransport");
        if ( !sourceName.isEmpty() ) {
            engine->disconnectSource( sourceName, q );
        }

        state = StopLineEditPrivate::WaitingForStopSuggestions;
        sourceName = QString("Stops %1|stop=%2").arg( serviceProvider, stopName );
        if ( !city.isEmpty() ) { // m_useSeparateCityValue ) {
            sourceName += QString("|city=%3").arg( city );
        }
        engine->connectSource( sourceName, q );
    };

    StopList stops;
    QString city;
    QString serviceProvider;
    QString providerType;
    State state;
    int progress; // Progress of the data engine in processing a task (0 .. 100)
    QString sourceName; // Source name used to request stop suggestions at the data engine
    QString errorString;
    QString question;
    QString questionToolTip;
    QString infoMessage;
    QList<Button> buttons;
    Plasma::ServiceJob *importJob; // A currently running import job or 0 if no import is running

#ifdef MARBLE_FOUND
    PublicTransportMapWidget *mapPopup;
#endif

protected:
    StopLineEdit* const q_ptr;
};
Q_DECLARE_OPERATORS_FOR_FLAGS( StopLineEditPrivate::ButtonTypes );
Q_DECLARE_OPERATORS_FOR_FLAGS( StopLineEditPrivate::ButtonStates );

StopLineEdit::StopLineEdit( QWidget* parent, const QString &serviceProvider,
                            KGlobalSettings::Completion completion )
        : KLineEdit( parent ), d_ptr( new StopLineEditPrivate(serviceProvider, this) )
{
#ifdef MARBLE_FOUND
    Q_D( StopLineEdit );
    d->mapPopup = new PublicTransportMapWidget( serviceProvider,
                                                PublicTransportMapWidget::DefaultFlags, this );
    connect( d->mapPopup, SIGNAL(stopClicked(Stop)), this, SLOT(stopClicked(Stop)) );
    PopupHandler *handler = PopupHandler::installPopup( d->mapPopup, this );
    connect( handler, SIGNAL(popupHidden()), this, SLOT(popupHidden()) );
#endif

    setCompletionMode( completion );
    connect( this, SIGNAL(textEdited(QString)), this, SLOT(edited(QString)) );
    connect( this, SIGNAL(textChanged(QString)), this, SLOT(changed(QString)) );
}

StopLineEdit::~StopLineEdit()
{
    delete d_ptr;
}

#ifdef MARBLE_FOUND
void StopLineEdit::popupHidden()
{
    setFocus( Qt::PopupFocusReason );
}

void StopLineEdit::stopClicked( const Stop &stop )
{
    if ( stop.isValid() ) {
        setText( stop.name );
    }
}
#endif

void StopLineEdit::setServiceProvider( const QString& serviceProvider )
{
    Q_D( StopLineEdit );
    Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine("publictransport");
    if ( !d->serviceProvider.isEmpty() ) {
        engine->disconnectSource( "ServiceProvider " + d->serviceProvider, this );
    }
    d->serviceProvider = serviceProvider;
    d->state = StopLineEditPrivate::Ready;
    if ( !d->sourceName.isEmpty() ) {
        engine->disconnectSource( d->sourceName, this );
    }

#ifdef MARBLE_FOUND
    d->mapPopup->setServiceProvider( serviceProvider );
#endif

    const Plasma::DataEngine::Data providerData =
            engine->query( "ServiceProvider " + d->serviceProvider );
    d->providerType = providerData["type"].toString();
    setEnabled( true );
    setClearButtonShown( true ); // Stays enabled and does not get drawn in KLineEdit::paintEvent
    setReadOnly( false );
    completionObject()->clear();
    edited( text() );

    // Check and watch the provider state
    if ( d->providerType == QLatin1String("GTFS") ) {
        if ( providerData["state"] == QLatin1String("gtfs_feed_import_pending") ) {
            askToImportGtfsFeed();
        }
        engine->connectSource( "ServiceProvider " + d->serviceProvider, this );
    }
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
        Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine("publictransport");
        engine->disconnectSource( d->sourceName, this );
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

void StopLineEdit::importGtfsFeed()
{
    Q_D( StopLineEdit );
    if ( d->providerType != QLatin1String("GTFS") ) {
        kWarning() << "Not a GTFS provider";
        return;
    }

    kDebug() << "GTFS accessor whithout imported feed data, use service to import using the GTFS feed";
    Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine("publictransport");
    Plasma::Service *service = engine->serviceForSource("GTFS");
    KConfigGroup op = service->operationDescription("importGtfsFeed");
    op.writeEntry( "serviceProviderId", d->serviceProvider );

    d->state = StopLineEditPrivate::WaitingForImport;
    d->addButtons( StopLineEditPrivate::StopButton | StopLineEditPrivate::PauseButton );
    d->updateButtonRects( contentsRect() );
    d->progress = 0;
    d->infoMessage.clear();
    setEnabled( true );
    setClearButtonShown( false ); // Stays enabled and does not get drawn in KLineEdit::paintEvent
    setReadOnly( true );

    // Update the progress bar
    update();

    d->importJob = service->startOperationCall( op );
    connect( d->importJob, SIGNAL(finished(KJob*)), this, SLOT(importFinished(KJob*)) );
}

bool StopLineEdit::cancelImport()
{
    Q_D( StopLineEdit );
    if ( d->importJob ) {
        return d->importJob->kill( KJob::EmitResult );
    }
    return false;
}

void StopLineEdit::changed( const QString &newText )
{
#ifdef MARBLE_FOUND
    Q_D( StopLineEdit );
    if ( newText.isEmpty() ) {
        kDebug() << "Empty, hide map";
        d->mapPopup->hide();
    } else if ( d->mapPopup->isVisible() ) {
        d->mapPopup->publicTransportLayer()->setSelectedStop( newText );
    }
#endif
}

void StopLineEdit::edited( const QString& newText )
{
    Q_D( StopLineEdit );

    // Do not connect new sources, if the data engine indicated that it is currently processing
    // a task (ie. download a GTFS feed or import it into the database)
    if ( d->state == StopLineEditPrivate::WaitingForImport ) {
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

    d->connectStopsSource( newText );
}

void StopLineEdit::paintEvent( QPaintEvent *ev )
{
    Q_D( StopLineEdit );

    if ( !paintEngine() ) {
        kDebug() << "No paint engine";
        return;
    }

    if ( d->state == StopLineEditPrivate::WaitingForImport ) {
        // Draw a progress bar while waiting for the data engine to complete a task
        QPainter painter( this );
        int buttonsWidth = d->drawAdditionalButtons( &painter );

        QStyleOptionProgressBar option;
        option.initFrom( this );
        option.minimum = 0;
        option.maximum = 100;
        option.progress = d->progress;
        option.text = d->infoMessage.isEmpty()
                ? i18nc("@info/plain", "Loading GTFS feed... %1 %", d->progress)
                : QString("%1... %2 %").arg(d->infoMessage).arg(d->progress);
        option.textAlignment = Qt::AlignCenter;
        option.textVisible = true;
        option.rect.setWidth( option.rect.width() - buttonsWidth );
        style()->drawControl( QStyle::CE_ProgressBar, &option, &painter );
    } else if ( d->state == StopLineEditPrivate::Error || d->state == StopLineEditPrivate::Pause ) {
        const QColor errorColor = KColorScheme( QPalette::Normal, KColorScheme::View )
                .foreground( KColorScheme::NegativeText ).color();
        QStyleOptionFrameV3 option;
        option.initFrom( this );
        option.frameShape = QFrame::StyledPanel;
        option.state = QStyle::State_Sunken;
        option.features = QStyleOptionFrameV2::None;
        option.lineWidth = 1;
        option.midLineWidth = 1;
        option.palette.setColor( QPalette::All, QPalette::Base,
                KColorScheme(QPalette::Normal, KColorScheme::View).background(
                KColorScheme::NegativeBackground).color()  );

        QPainter painter( this );
        style()->drawPrimitive( QStyle::PE_PanelLineEdit, &option, &painter, this );
        style()->drawPrimitive( QStyle::PE_FrameLineEdit, &option, &painter, this );

        int buttonsWidth = 0;
        QString errorText;
        if ( d->state == StopLineEditPrivate::Error ) {
            errorText = d->errorString.isEmpty()
                    ? i18nc("@info/plain", "Error loading the Service Provider") : d->errorString;
        } else { // Pause state
            errorText = i18nc("@info/plain", "Import paused");
            buttonsWidth = d->drawAdditionalButtons( &painter );
        }
        painter.setPen( errorColor );
        painter.drawText( contentsRect(), Qt::AlignCenter,
                          fontMetrics().elidedText(errorText, Qt::ElideMiddle,
                                                   option.rect.width() - buttonsWidth - 4) );
    } else if ( d->state == StopLineEditPrivate::AskingToImportGtfsFeed ) {
        const QColor infoColor = KColorScheme( QPalette::Normal, KColorScheme::View )
                .foreground( KColorScheme::PositiveText ).color();
        QStyleOptionFrameV3 option;
        option.initFrom( this );
        option.frameShape = QFrame::StyledPanel;
        option.state = QStyle::State_Sunken;
        option.features = QStyleOptionFrameV2::None;
        option.lineWidth = 1;
        option.midLineWidth = 1;
        option.palette.setColor( QPalette::All, QPalette::Base,
                KColorScheme(QPalette::Normal, KColorScheme::View).background(
                KColorScheme::PositiveBackground).color()  );

        QPainter painter( this );
        style()->drawPrimitive( QStyle::PE_PanelLineEdit, &option, &painter, this );
        style()->drawPrimitive( QStyle::PE_FrameLineEdit, &option, &painter, this );

        int buttonsWidth = d->drawAdditionalButtons( &painter );
        QRect textRect = option.rect;
        textRect.setWidth( textRect.width() - buttonsWidth );
        painter.setPen( infoColor );
        painter.drawText( textRect, Qt::AlignCenter,
                fontMetrics().elidedText(d->question, Qt::ElideMiddle, textRect.width() - 4) );
    } else {
        KLineEdit::paintEvent( ev );
    }
}

void StopLineEdit::resizeEvent( QResizeEvent *ev )
{
    Q_D( StopLineEdit );

    KLineEdit::resizeEvent( ev );
    d->updateButtonRects( contentsRect() );
}

void StopLineEdit::mousePressEvent(QMouseEvent* ev)
{
    Q_D( StopLineEdit );

    // Handle clicks on the import gtfs feed buttons (ok, cancel)
    if ( ev->button() == Qt::LeftButton ) {
        StopLineEditPrivate::Button *button = d->hoveredButton();
        if ( button ) {
            // Button pressed
            button->state |= StopLineEditPrivate::ButtonPressed;
            update( button->rect.adjusted(0, 0, 1, 1) );
            return;
        }
    }

    KLineEdit::mousePressEvent(ev);
}

void StopLineEdit::mouseReleaseEvent( QMouseEvent *ev )
{
    Q_D( StopLineEdit );

    // Handle clicks on the import gtfs feed buttons (ok, cancel)
    if ( ev->button() == Qt::LeftButton ) {
        for ( int i = 0; i < d->buttons.count(); ++i ) {
            d->buttons[i].state &= ~StopLineEditPrivate::ButtonPressedMouseOutside;
        }

        StopLineEditPrivate::Button *button = d->hoveredButton();
        if ( button ) {
            button->state &= ~StopLineEditPrivate::ButtonPressed;
            if ( d->state == StopLineEditPrivate::AskingToImportGtfsFeed &&
                 button->type == StopLineEditPrivate::StartButton )
            {
                // Start button released
                d->state = StopLineEditPrivate::Ready;
                if ( toolTip() == d->questionToolTip ) {
                    // Clear tooltip, if the question was still set as tooltip
                    setToolTip( QString() );
                }
                d->question.clear();
                d->questionToolTip.clear();

                // Start the import
                importGtfsFeed();
                return;
            } else if ( d->importJob && (d->state == StopLineEditPrivate::WaitingForImport ||
                        d->state == StopLineEditPrivate::Pause) &&
                        button->type == StopLineEditPrivate::StopButton )
            {
                // Cancel button released, cancel the import
                if ( !cancelImport() ) {
                    kWarning() << "Could not cancel the import job";
                    return;
                }
                d->question.clear();
                d->questionToolTip.clear();
                update();
                return;
            } else if ( d->importJob && (d->state == StopLineEditPrivate::WaitingForImport ||
                        d->state == StopLineEditPrivate::Pause) &&
                        button->type == StopLineEditPrivate::PauseButton )
            {
                // Pause button released in importing or pause state. In pause state the button
                // is used as continue button.
                if ( d->importJob->isSuspended() ) {
                    // Import job is currently suspended (pause button is a start button)
                    // Try to resume it again
                    if ( !d->importJob->resume() ) {
                        kDebug() << "Could not resume the import job";
                        return;
                    }
                    d->state = StopLineEditPrivate::WaitingForImport; // Back to importing state
                    button->icon = KIcon("media-playback-pause"); // Set icon back to pause
                    setToolTip( i18nc("@info:tooltip Tooltip for StopLineEdits, ie. shown in the "
                                      "stop settings dialog for stop name input. This tooltip is "
                                      "used, when the GTFS feed for the current service provider "
                                      "currently gets imported.",
                                      "Importing the GTFS feed for stop suggestions, %1%% done. "
                                      "Please wait.", d->progress) );
                } else {
                    // Import job is not currently suspended, try to suspend it
                    if ( !d->importJob->suspend() ) {
                        kDebug() << "Could not suspend the import job";
                        return;
                    }
                    d->state = StopLineEditPrivate::Pause; // Set pause state
                    button->icon = KIcon("media-playback-start"); // Use start icon for continue
                    setToolTip( i18nc("@info:tooltip Tooltip for StopLineEdits, ie. shown in the "
                                      "stop settings dialog for stop name input. This tooltip is "
                                      "used, when a currently running GTFS feed import job is "
                                      "suspended.",
                                      "Importing the GTFS feed for stop suggestions, "
                                      "%1%% done (paused)", d->progress) );
                }

                update();
            }
        }
    }

    KLineEdit::mousePressEvent( ev );
}

void StopLineEdit::mouseMoveEvent( QMouseEvent *ev )
{
    Q_D( StopLineEdit );

    // Handle clicks on the import gtfs feed buttons (ok, cancel)
    for ( int i = 0; i < d->buttons.count(); ++i ) {
        d->buttons[i].state &= ~StopLineEditPrivate::ButtonHovered;

        // Change all pressed button states to pressed (mouse outside)
        if ( d->buttons[i].state.testFlag(StopLineEditPrivate::ButtonPressed) ) {
            d->buttons[i].state &= ~StopLineEditPrivate::ButtonPressed;
            d->buttons[i].state |= StopLineEditPrivate::ButtonPressedMouseOutside;
            update( d->buttons[i].rect.adjusted(0, 0, 1, 1) );
        }
    }

    int index;
    StopLineEditPrivate::Button *button = d->buttonAt( ev->pos(), &index );
    if ( button ) {
        // Mouse moved over a button
        if ( !ev->buttons().testFlag(Qt::LeftButton) ) {
            // Remove pressed state
            button->state |= StopLineEditPrivate::ButtonHovered;
            button->state &= ~StopLineEditPrivate::ButtonPressedMouseOutside;
            button->state &= ~StopLineEditPrivate::ButtonPressed;
            update( button->rect.adjusted(0, 0, 1, 1) );
        } else if ( button->state.testFlag(StopLineEditPrivate::ButtonPressedMouseOutside) ) {
            // Change intermediate pressed state (mouse outside button) back to pressed state
            button->state |= StopLineEditPrivate::ButtonHovered;
            button->state &= ~StopLineEditPrivate::ButtonPressedMouseOutside;
            button->state |= StopLineEditPrivate::ButtonPressed;
            update( button->rect.adjusted(0, 0, 1, 1) );
        }
    } else {
        QLineEdit::mouseMoveEvent( ev );
    }
}

bool StopLineEdit::event( QEvent *ev )
{
    Q_D( StopLineEdit );

    StopLineEditPrivate::Button *button = d->hoveredButton();
    switch ( ev->type() ) {
    case QEvent::ToolTip:
        if ( button ) {
            // Mouse is over a button
            setToolTip( button->toolTip );
        } else if ( d->providerType == QLatin1String("GTFS") && !d->questionToolTip.isEmpty() ) {
            // Reset tooltip to question
            setToolTip( d->questionToolTip );
        }
        break;
    default:
        break;
    }

    return KLineEdit::event( ev );
}

void StopLineEdit::importFinished( KJob *job )
{
    Q_D( StopLineEdit );
    kDebug() << "Finished GTFS feed import" << job->errorString();
    d->importJob = 0;
    const bool hasError = job->error() < 0 && job->error() != -2; // -2 => Not a GTFS provider
    d->state = hasError ? StopLineEditPrivate::Error : StopLineEditPrivate::Ready;
    d->errorString = job->errorString(); // TODO needed?
    setToolTip( job->errorString() );
    setClearButtonShown( !hasError );
    setReadOnly( hasError );

    // TODO Re-request stop suggestions now?
}

void StopLineEdit::askToImportGtfsFeed()
{
    Q_D( StopLineEdit );

    if ( d->state == StopLineEditPrivate::AskingToImportGtfsFeed ) {
        // Was already asking
        return;
    }

    d->state = StopLineEditPrivate::AskingToImportGtfsFeed;
    // Add a button to start the import
    d->addButtons( StopLineEditPrivate::StartButton );
    d->updateButtonRects( contentsRect() );
    d->question = i18nc("@info/plain", "Start GTFS feed import?");
    d->questionToolTip = i18nc("@info:tooltip", "<title>Start GTFS feed import?</title>"
                      "<para>Click the start button to begin to import the timetable data, which may "
                      "take some minutes (depending on the size of the GTFS feed). <nl/>"
                      "The import can be suspended or cancelled at any time. <nl/>"
                      "After the import, updates are only done if the GTFS feed changes. "
                      "Realtime data gets retrieved using GTFS-realtime automatically if "
                      "supported, other than that GTFS data sources can be used offline.</para>");
    setToolTip( d->questionToolTip );

    // Do not show the clear button over the start button
    setClearButtonShown( false );
    setReadOnly( true );
}

void StopLineEdit::showGtfsFeedImportProgress()
{
    Q_D( StopLineEdit );

    if ( d->state == StopLineEditPrivate::WaitingForImport ) {
        // Was already showing import progress
        return;
    }

    d->state = StopLineEditPrivate::WaitingForImport;
    d->addButtons( StopLineEditPrivate::StopButton | StopLineEditPrivate::PauseButton );
    d->updateButtonRects( contentsRect() );
    setClearButtonShown( false ); // Stays enabled and does not get drawn in KLineEdit::paintEvent
    setReadOnly( true );
}

void StopLineEdit::dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data& data )
{
    Q_D( StopLineEdit );

    if ( sourceName == "ServiceProvider " + d->serviceProvider ) {
        // The service provider state has changed
        const QString state = data[ "state" ].toString();
        if ( state == QLatin1String("ready") ) {
            d->state = StopLineEditPrivate::Ready;
            setClearButtonShown( true );
            setReadOnly( false );
        } else {
            const QVariantHash stateData = data[ "stateData" ].toHash();
            if ( state == QLatin1String("gtfs_feed_import_pending") ) {
                askToImportGtfsFeed();
            } else if ( state == QLatin1String("importing_gtfs_feed") ) {
                showGtfsFeedImportProgress();

                // Update the progress bar
                d->progress = stateData["progress"].toInt();
                d->infoMessage = stateData["statusMessage"].toString();
                update();
            }
        }
        return;
    }

    if ( !sourceName.startsWith(QLatin1String("Stops")) || d->sourceName != sourceName ) {
        kDebug() << "Wrong (old) source" << sourceName;
        return;
    }

    kDebug() << "Updated Data For Source" << sourceName;

    // Check for a special error code ErrorNeedsImport
    if ( data["error"].toBool() && data["errorCode"].toInt() == 3 ) { // 3 -> ErrorNeedsImport
        askToImportGtfsFeed();
        return;
    }

    if ( !d->sourceName.isEmpty() ) {
        Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine("publictransport");
        engine->disconnectSource( d->sourceName, this );
        d->sourceName.clear();
    }

    if ( data.value("error").toBool() ) {
        kDebug() << "Stop suggestions error" << sourceName;
        d->state = StopLineEditPrivate::Error;
    } else if ( !data.contains("stops") ) {
        kDebug() << "No stop suggestions received" << sourceName;
        d->state = StopLineEditPrivate::Error;
    } else {
        d->state = StopLineEditPrivate::Ready;
    }

    setClearButtonShown( d->state != StopLineEditPrivate::Error );
    setReadOnly( d->state == StopLineEditPrivate::Error );
    if ( d->state == StopLineEditPrivate::Error ) {
        return;
    }

    // Delete old stops
    QList<Stop>::Iterator it = d->stops.begin();
    while ( it != d->stops.end() ) {
        if ( it->name.contains(text()) ) {
            ++it;
        } else {
            // The stop name does not contain the text in this StopLineEdit, remove it
            it = d->stops.erase( it );
        }
    }

    QStringList weightedStops;
    QHash<Stop, QVariant> stopToStopWeight;
    QVariantList stops = data["stops"].toList();
    foreach ( const QVariant &stopData, stops ) {
        QVariantHash stop = stopData.toHash();
        const QString stopName = stop["StopName"].toString();
        const QString stopID = stop["StopID"].toString();
        const int stopWeight = stop["StopWeight"].toInt();
        const bool coordsAvailable = stop.contains("StopLongitude") && stop.contains("StopLatitude");
        const qreal longitude = coordsAvailable ? stop["StopLongitude"].toReal() : 0;
        const qreal latitude = coordsAvailable? stop["StopLatitude"].toReal() : 0;

        const Stop stopItem( stopName, stopID, coordsAvailable, longitude, latitude );
        stopToStopWeight.insert( stopItem, stopWeight );
        d->stops << stopItem;
    }
    // Construct weighted stop list for KCompletion
    bool hasAtLeastOneWeight = false;
    foreach ( const Stop &stop, d->stops ) {
        int stopWeight = stopToStopWeight[ stop.name ].toInt();
        if ( stopWeight <= 0 ) {
            stopWeight = 0;
        } else {
            hasAtLeastOneWeight = true;
        }

        weightedStops << QString( "%1:%2" ).arg( stop.name ).arg( stopWeight );
    }

    // Only add stop suggestions, if the line edit still has focus
    if ( hasFocus() ) {
        kDebug() << "Prepare completion object";
        KCompletion *comp = completionObject();
        comp->setIgnoreCase( true );
        comp->clear();
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

        // Complete manually, because the completions are requested asynchronously.
        // Remove selected text for automatic completion modes, to still show other suggestions
        // when text gets autocompleted
        const bool autoCompletion = completionMode() == KGlobalSettings::CompletionAuto ||
                                    completionMode() == KGlobalSettings::CompletionPopupAuto;
        const QString input = (!autoCompletion || !hasSelectedText()) ? text()
                : text().remove(selectionStart(), selectedText().length());
        doCompletion( input );

#ifdef MARBLE_FOUND
        const QStringList activeStopNames = completionObject()->substringCompletion( input );
        d->mapPopup->setStops( d->stops, text(), activeStopNames );
        if ( !d->mapPopup->publicTransportLayer()->stops().isEmpty() ) {
            d->mapPopup->show();
        }
#endif
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

} // namespace Timetable
