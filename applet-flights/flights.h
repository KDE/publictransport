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

// Here we avoid loading the header multiple times
#ifndef FLIGHTS_HEADER
#define FLIGHTS_HEADER

// We need the Plasma Applet headers
#include <Plasma/PopupApplet>
#include <Plasma/DataEngine>

namespace Plasma {
class Label;
}

class FlightDepartureList;
class StopLineEdit;

// Define our plasma Applet
class Flights : public Plasma::PopupApplet
{
	Q_OBJECT
public:
	// Basic Create/Destroy
	Flights(QObject *parent, const QVariantList &args);
	~Flights();

	// The paintInterface procedure paints the applet to the desktop
// 	void paintInterface(QPainter *painter,
// 			const QStyleOptionGraphicsItem *option,
// 			const QRect& contentsRect);
	
	
	void init();
	
	virtual void createConfigurationInterface( KConfigDialog* parent );
	virtual QGraphicsWidget* graphicsWidget();
	virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
	
public slots:
	void configAccepted();
	
	/** @brief The data from the data engine was updated. */
	void dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data &data );
	
private:
	StopLineEdit *m_stopLineEdit;
	FlightDepartureList *m_flightDepartureList;
	QString m_airport;
	Plasma::Label *m_header;
	QGraphicsWidget *m_container;
};
 
// This is the command that links your applet to the .desktop file
K_EXPORT_PLASMA_APPLET(flights, Flights)
#endif
