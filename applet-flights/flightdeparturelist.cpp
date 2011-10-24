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

// Header
#include "flightdeparturelist.h"

// Plasma includes
#include <Plasma/ScrollWidget>
#include <Plasma/Theme>
#include <Plasma/Svg>
#include <Plasma/FrameSvg>
#include <Plasma/Label>
#include <Plasma/IconWidget>

// Qt includes
#include <QStyleOption>
#include <QPainter>
#include <QGraphicsLinearLayout>
#include <QGraphicsGridLayout>
#include <QLabel>
#include <qmath.h>
#include <global.h>

FlightDeparture::FlightDeparture( QGraphicsItem* parent ) : QGraphicsWidget( parent )
{
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	setMinimumSize( 125, 45 );

	m_icon = new Plasma::IconWidget( Timetable::Global::vehicleTypeToIcon(Timetable::Plane),
                                     QString(), this );
	m_header = new Plasma::Label( this );
	m_info = new Plasma::Label( this );

	m_icon->setMinimumSize( 32, 32 );
	m_icon->setMaximumSize( 32, 32 );

	QFont headerFont = m_header->font();
	headerFont.setBold( true );
	m_header->setFont( headerFont );
	m_header->setText( headerText() );
	m_header->setToolTip( headerText() );
	m_header->setMaximumHeight( m_header->boundingRect().height() * 0.9 );

	m_info->setText( infoText() );
	m_info->setToolTip( infoText() );
	#if KDE_VERSION >= KDE_MAKE_VERSION(4,5,0)
		m_info->setWordWrap( true );
	#endif
	m_info->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	m_info->setMaximumHeight( boundingRect().height() - m_header->maximumHeight() - 5 );

	QGraphicsGridLayout *mainLayout = new QGraphicsGridLayout( this );
	mainLayout->addItem( m_icon, 0, 0, 2, 1, Qt::AlignCenter );
	mainLayout->addItem( m_header, 0, 1, 1, 1, Qt::AlignBottom );
	mainLayout->addItem( m_info, 1, 1, 1, 1, Qt::AlignTop );
	mainLayout->setHorizontalSpacing( 10 );
	mainLayout->setVerticalSpacing( 0 );
	mainLayout->setContentsMargins( 0, 0, 0, 0 );
}

QString FlightDeparture::headerText() const
{
	return i18n("Flight %1 to %2", m_flightNumber, m_target);
}

QString FlightDeparture::infoText() const
{
	return i18n("Departing at %1, %2, %3",
			KGlobal::locale()->formatTime(m_departure.time()), m_status, m_airline);
}

void FlightDeparture::setTarget( const QString& target )
{
	// Set target and remove the shorthand of the airport
	m_target = target;
	m_target.replace( QRegExp("^[A-Z]+\\s"), QString() );
	m_header->setText( headerText() );
	m_header->setToolTip( headerText() );
}

void FlightDeparture::setDeparture( const QDateTime& departure )
{
	m_departure = departure;
	m_info->setText( infoText() );
	m_info->setToolTip( infoText() );
}

void FlightDeparture::setAirline( const QString& airline )
{
	m_airline = airline;
	m_info->setText( infoText() );
	m_info->setToolTip( infoText() );
}

void FlightDeparture::setFlightNumber( const QString& flightNumber )
{
	m_flightNumber = flightNumber;
	m_header->setText( headerText() );
	m_header->setToolTip( headerText() );
}

void FlightDeparture::setStatus( const QString& status )
{
	m_status = status;
	m_info->setText( infoText() );
	m_info->setToolTip( infoText() );
}

void FlightDeparture::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
							 QWidget* widget )
{
	painter->setRenderHints( QPainter::SmoothPixmapTransform | QPainter::Antialiasing );

	QGraphicsWidget::paint( painter, option, widget );

	QString headerText = i18n("Flight %1 to %2", m_flightNumber, m_target);
	QString text = i18n("Departing at %1, %2, %3",
						KGlobal::locale()->formatTime(m_departure.time()), m_status, m_airline);

	int iconSize = qMin( qMax(qCeil(option->rect.height() / 2), 16), 64 );
	QRectF iconRect( 0, (option->rect.height() - iconSize) / 2, iconSize, iconSize );
	QRectF headerRect( 5 + iconSize, 0,
					   option->rect.width() - 5 - iconSize, option->rect.height() / 3 );
	QRectF textRect( 5 + iconSize, headerRect.height(),
					 headerRect.width(), 2 * option->rect.height() / 3 );

// 	QColor textColor = option->palette.text().color();
// 	bool drawHalos = qGray(textColor.rgb()) < 192;
//
// 	QList< QRect > fadeRects, haloRects;
// 	int fadeWidth = 30;
// 	QPixmap pixmap( textRect.size() );
// 	pixmap.fill( Qt::transparent );
// 	QPainter p( &pixmap );
// 	p.setPen( painter->pen() );

    Plasma::FrameSvg svg(this);
	QRectF backgroundRect = option->rect.adjusted( -12, -12, 12, 12 );
	svg.setImagePath("widgets/background");
	svg.resizeFrame( backgroundRect.size() );
	svg.paintFrame( painter, backgroundRect.topLeft() );
// 	painter->drawRect( option->rect );
	return;

	QFont normalFont = font();
	QFont headerFont = normalFont;
	headerFont.setBold( true );

	QPixmap icon = Timetable::Global::vehicleTypeToIcon( Timetable::Plane )
            .pixmap( iconRect.size().toSize() );
	painter->drawPixmap( iconRect.topLeft(), icon );

	QRadialGradient headerGradient( 50.0, 50.0, qMax(headerRect.height(), qreal(100.0)) );
	QColor color = Plasma::Theme::defaultTheme()->color(Plasma::Theme::HighlightColor);
	color.setAlphaF( 0.4 );
	headerGradient.setColorAt( 0, color );
	headerGradient.setColorAt( 1, Qt::transparent );
	painter->fillRect( headerRect, QBrush(headerGradient) );

	QLinearGradient gradient( headerRect.bottomLeft(), headerRect.bottomRight() );
	gradient.setColorAt( 0, Qt::black );
	gradient.setColorAt( 1, Qt::transparent );
	painter->fillRect( headerRect.left(), headerRect.bottom(), headerRect.width(), 1,
					   QBrush(gradient) );

	painter->setFont( headerFont );
	QFontMetrics fmHeader( headerFont );
	headerText = fmHeader.elidedText( headerText, Qt::ElideRight, headerRect.width() );
	painter->drawText( headerRect, headerText, QTextOption(Qt::AlignBottom) );

	painter->setFont( normalFont );
	QFontMetrics fmText( normalFont );
	text = fmHeader.elidedText( text, Qt::ElideRight, textRect.width() * 2.1 );
	painter->drawText( textRect, text, QTextOption(Qt::AlignTop) );
}

FlightDepartureList::FlightDepartureList( QGraphicsItem* parent, Qt::WindowFlags wFlags )
		: QGraphicsWidget( parent, wFlags ), m_contentWidget(0)
{
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

	Plasma::ScrollWidget *scrollWidget = new Plasma::ScrollWidget( this );
	m_contentWidget = new QGraphicsWidget( scrollWidget );
	m_contentWidget->setContentsMargins( 10, 10, 10, 10 );
	scrollWidget->setWidget( m_contentWidget );

	QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout( this );
	mainLayout->addItem( scrollWidget );

	QGraphicsLinearLayout *contentLayout = new QGraphicsLinearLayout( Qt::Vertical, m_contentWidget );
	contentLayout->setSpacing( 10 );
}

void FlightDepartureList::updateLayout()
{
	QGraphicsLinearLayout *contentLayout = new QGraphicsLinearLayout( Qt::Vertical, m_contentWidget );
	contentLayout->setSpacing( 10 );

	int maxDepartures = qCeil( boundingRect().height() / 100 ); // Min 100 pixel per departure
	for ( int i = 0; i < departures().count(); ++i ) {
		FlightDeparture *departure = departures()[i];
		departure->setVisible( i < maxDepartures );
		if ( departure->isVisible() ) {
			contentLayout->addItem( departure );
		}
	}
}

void FlightDepartureList::setTimetableData( const Plasma::DataEngine::Data& data )
{
	QGraphicsLinearLayout *contentLayout = new QGraphicsLinearLayout( Qt::Vertical, m_contentWidget );
	contentLayout->setSpacing( 10 );

	qDeleteAll( m_departures );
	m_departures.clear();

	QUrl url;
	QDateTime updated;
	url = data["requestUrl"].toUrl();
	updated = data["updated"].toDateTime();
	int count = qMin( 10, data["count"].toInt() );
	kDebug() << "  - " << count << "departures to be processed";
	for ( int i = 0; i < count; ++i ) {
		QVariant departureData = data.value( QString::number(i) );
		// Don't process invalid data
		if ( !departureData.isValid() ) {
			kDebug() << "Departure data for departure" << i << "is invalid" << data;
			continue;
		}

		QVariantHash dataMap = departureData.toHash();
		FlightDeparture *flightDeparture = new FlightDeparture( this );
		flightDeparture->setDeparture( dataMap["departure"].toDateTime() );
		flightDeparture->setAirline( dataMap["operator"].toString() );
		flightDeparture->setTarget( dataMap["target"].toString() );
		flightDeparture->setFlightNumber( dataMap["line"].toString() );
		flightDeparture->setStatus( dataMap["status"].toString().replace(QRegExp("&nbsp;|\n"), QString()) );
		m_departures << flightDeparture;

		contentLayout->addItem( flightDeparture );
// 		QList< QTime > routeTimes;
// 		if ( dataMap.contains( "routeTimes" ) ) {
// 			QVariantList times = dataMap[ "routeTimes" ].toList();
// 			foreach( const QVariant &time, times ) {
// 				routeTimes << time.toTime();
// 			}
// 		}
// 		DepartureInfo departureInfo( dataMap["operator"].toString(), dataMap["line"].toString(),
// 				dataMap["target"].toString(), dataMap["departure"].toDateTime(),
// 				static_cast<VehicleType>( dataMap["vehicleType"].toInt() ),
// 				dataMap["nightline"].toBool(), dataMap["expressline"].toBool(),
// 				dataMap["platform"].toString(), dataMap["delay"].toInt(),
// 				dataMap["delayReason"].toString(), dataMap["journeyNews"].toString(),
// 				dataMap["routeStops"].toStringList(), routeTimes,
// 				dataMap["routeExactStops"].toInt() );
	} // for ( int i = 0; i < count; ++i )

	update();
}
