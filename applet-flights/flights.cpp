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
#include "flights.h"
#include "flightdeparturelist.h"

// libpublictransporthelper includes
#include <stopwidget.h>
#include <stoplineedit.h>
#include <stopsettings.h>
#include <global.h>

// KDE includes
#include <KLocale>
#include <KConfigDialog>
#include <Plasma/Label>
#include <plasma/theme.h>

// Qt includes
#include <QPainter>
#include <QFontMetrics>
#include <QSizeF>
#include <QFormLayout>
#include <QGraphicsLinearLayout>
#include <qmath.h>

using namespace PublicTransport;

Flights::Flights(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args), m_stopLineEdit(0), m_flightDepartureList(0), m_header(0),
    m_container(0)
{
    setBackgroundHints( DefaultBackground );
//     m_svg.setImagePath("widgets/background");
    setHasConfigurationInterface( true );
    setContentsMargins( 10, 10, 10, 10 );
    setAspectRatioMode( Plasma::IgnoreAspectRatio );
    resize( 300, 200 );
    setPopupIcon( PublicTransport::Global::vehicleTypeToIcon(Plane) );
}

Flights::~Flights()
{
    if ( hasFailedToLaunch() ) {
        // Do some cleanup here
    } else {
        // Save settings
    }
}

void Flights::init()
{
    m_airport = config().readEntry( QLatin1String("airport"), QString() );
    setConfigurationRequired( m_airport.isEmpty(), i18n("Please select an airport") );
    if ( !m_airport.isEmpty() ) {
        dataEngine("publictransport")->connectSource(
                QString("Departures international_flightstats|stop=%1|timeoffset=0").arg(m_airport), this,
                60000, Plasma::AlignToMinute );
    }
}

void Flights::resizeEvent( QGraphicsSceneResizeEvent* event )
{
    Plasma::Applet::resizeEvent( event );

//     if ( m_flightDepartureList ) {
//         m_flightDepartureList->updateLayout();
//     }
}

QGraphicsWidget* Flights::graphicsWidget()
{
    if ( !m_container ) {
        m_container = new QGraphicsWidget( this );

        m_header = new Plasma::Label( m_container );
        m_header->setText( m_airport );
        QFont font = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
        font.setPointSize( 14 );
        m_header->setFont( font );
        m_header->setAlignment( Qt::AlignCenter );

        m_flightDepartureList = new FlightDepartureList( m_container );
        m_flightDepartureList->setPreferredSize( 300, 200 );

        QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout( this );
        mainLayout->addItem( m_container );
        mainLayout->setContentsMargins( 0, 0, 0, 0 );

        QGraphicsLinearLayout *containerLayout = new QGraphicsLinearLayout( Qt::Vertical, m_container );
        containerLayout->addItem( m_header );
        containerLayout->addItem( m_flightDepartureList );
        containerLayout->setContentsMargins( 0, 4, 0, 0 );
        containerLayout->setSpacing( 0 );

        registerAsDragHandle( m_header );
        registerAsDragHandle( m_flightDepartureList );
    }

    return m_container;
}

void Flights::createConfigurationInterface( KConfigDialog* parent )
{
//     StopSettings stopSettings;
//     stopSettings.set( LocationSetting, "international" );
//     stopSettings.set( ServiceProviderSetting, "international_flightstats" );

    QWidget *airportConfig = new QWidget( parent );
    QFormLayout *airportLayout = new QFormLayout( airportConfig );
    m_stopLineEdit = new StopLineEdit( airportConfig, QLatin1String("international_flightstats") );
    m_stopLineEdit->setText( m_airport );
    airportLayout->addRow( i18n("&Airport:"), m_stopLineEdit );

    parent->addPage( airportConfig, i18n("Airport") );

    connect( parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()) );
    connect( parent, SIGNAL(okClicked()), this, SLOT(configAccepted()) );

    m_stopLineEdit->setFocus();
}

void Flights::configAccepted()
{
    m_airport = m_stopLineEdit->text();

    setConfigurationRequired( m_airport.isEmpty(), i18n("Please select an airport") );
    if ( !m_airport.isEmpty() ) {
        dataEngine("publictransport")->connectSource(
                QString("Departures international_flightstats|stop=%1").arg(m_airport), this,
                60000, Plasma::AlignToMinute );
    }

    config().writeEntry( QLatin1String("airport"), m_airport );
    emit configNeedsSaving();
    configChanged();
    m_stopLineEdit = 0;

    m_header->setText( m_airport );
}

void Flights::dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data& data )
{
    Q_UNUSED( sourceName );
    m_flightDepartureList->setTimetableData( data );
}

#include "flights.moc"
