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
#ifndef GRAPHICALTIMETABLELINE_HEADER
#define GRAPHICALTIMETABLELINE_HEADER

// libpublictransporthelper includes
#include <stopsettings.h>

// KDE includes
#include <Plasma/Applet>
#include <Plasma/DataEngine>
#include <KIcon>

#define DEPARTURE_SIZE 20
#define MIN_DISTANCE_BETWEEN_DEPARTURES 50
#define MIN_TIMELINE_LENGTH 5 // in minutes
#define MAX_TIMELINE_LENGTH 3 * 60 // in minutes

class QPropertyAnimation;
class QCheckBox;
class CheckCombobox;
namespace Plasma {
    class ToolButton;
    class Label;
};
namespace Timetable {
    class StopWidget;
    class VehicleTypeModel;
};
using namespace Timetable;

/**
 * @brief Data for one departure/arrival.
 **/
struct DepartureData {
	QDateTime time;
	QString transportLine;
	QString target;
	VehicleType vehicleType;
	bool drawTransportLine;

	DepartureData() {
		vehicleType = Unknown;
	};

	DepartureData( const QDateTime &time, const QString &transportLine, const QString &target,
				   VehicleType vehicleType, bool drawTransportLine = true ) {
		this->time = time;
		this->transportLine = transportLine;
		this->target = target;
		this->vehicleType = vehicleType;
		this->drawTransportLine = drawTransportLine;
	};

	bool operator ==( const DepartureData &other ) const {
		return time == other.time && transportLine == other.transportLine
				&& target == other.target && vehicleType == other.vehicleType;
	};
};

/**
 * @brief A QGraphicsItem showing one or more departures/arrivals.
 *
 * One Departure can be combined with another using @ref combineWith.
 **/
class Departure : public QGraphicsWidget {
	Q_OBJECT
	Q_PROPERTY( QSizeF size READ size WRITE setSize )
public:
    Departure( QGraphicsItem* parent, const DepartureData &data,
			   const QPointF &pos = QPointF() );
    Departure( QGraphicsItem* parent, const QList<DepartureData> &dataList,
			   const QPointF &pos = QPointF() );

	virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
						QWidget* widget = 0 );
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint = QSizeF()) const;
    virtual QRectF boundingRect() const;
	QSizeF size() const { return m_size; };
	void setSize( const QSizeF &size ) { m_size = size; updateGeometry(); update(); };

	/**
	 * @brief Updates the position based on the departure time.
	 *
	 * This function gets called automatically when the departure time gets changed using
	 * @ref setDateTime.
	 *
	 * @param animate Whether or not to animate the position change. The animation can take up to
	 *   one minute. Default is true.
	 **/
	QPointF updatePosition( bool animate = true );

	void updateTooltip();
	void updateDrawData();

	QList<DepartureData> departureData() const { return m_departures; };
	QDateTime dateTime() const { return m_departures.first().time; };
	QStringList transportLines() const {
		QStringList ret;
		foreach ( const DepartureData &data, m_departures ) {
			ret << data.transportLine;
		}
		return ret;
	};
	QStringList targets() const {
		QStringList ret;
		foreach ( const DepartureData &data, m_departures ) {
			ret << data.target;
		}
		return ret;
	};
	QList<VehicleType> vehicleTypes() const {
		QList<VehicleType> ret;
		foreach ( const DepartureData &data, m_departures ) {
			ret << data.vehicleType;
		}
		return ret;
	};

	bool containsDeparture( const DepartureData &other ) const {
		foreach ( const DepartureData &data, m_departures ) {
			if ( data == other ) {
				return true;
			}
		}

		return false; // Not found
	};

    void combineWith( Departure *other );
	Departure *splitAt( QGraphicsItem *parent, int index );

protected:
    virtual void hoverEnterEvent( QGraphicsSceneHoverEvent* event );

private:
	inline qreal departureSizeFactor() const {
		return m_drawData.count() == 1 ? 1.0 : 1.0 / (0.75 * m_drawData.count());
	};
	inline qreal departureOffset( qreal vehicleSize ) const {
		return m_drawData.count() == 1 ? 0.0
			: (boundingRect().width() - vehicleSize) / (m_drawData.count() - 1);
	};

	QList<DepartureData> m_departures; // Departures visualized by this item
	QList<DepartureData*> m_drawData; // Departures for which an own icon gets drawn
	QSizeF m_size;
};

// Define our plasma Applet
class GraphicalTimetableLine : public Plasma::Applet
{
	Q_OBJECT
public:
	// Basic Create/Destroy
	GraphicalTimetableLine( QObject *parent, const QVariantList &args );
	~GraphicalTimetableLine();

	virtual void createConfigurationInterface( KConfigDialog* parent );

	// The paintInterface procedure paints the applet to the desktop
	void paintInterface( QPainter *painter,
			const QStyleOptionGraphicsItem *option,
			const QRect& contentsRect );
	void init();

	QPointF positionFromTime( const QDateTime &time, qreal *opacity = 0, qreal *zoom = 0,
							  qreal *zValue = 0 ) const;
	void paintVehicle( QPainter *painter, VehicleType vehicle, const QRectF &rect,
					   const QString &transportLine = QString() );

    QString courtesyText();
	QDateTime endTime() const;
	void createTooltip( Departure *departure = 0 );

	QPointF newDeparturePosition() const { return m_timelineEnd; };

protected:
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event) ;

public slots:
	void configAccepted();

	/** @brief The data from the data engine was updated. */
	void dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data &data );

	void updateItemPositions( bool animate = true );
	void updateTitle();

	void zoomIn();
	void zoomOut();

private:
	// Configuration widgets
	StopWidget *m_stopWidget;
	VehicleTypeModel *m_vehicleTypeModel;
	QCheckBox *m_showTimetableCheckbox;
	QCheckBox *m_drawTransportLineCheckbox;

	// Settings
	StopSettings m_stopSettings;
	QList<VehicleType> m_vehicleTypes; // filter
	qreal m_timelineLength; // in minutes
	bool m_showTimetable;
	bool m_drawTransportLine;

	// Graphics items
	Plasma::ToolButton *m_zoomInButton;
	Plasma::ToolButton *m_zoomOutButton;
	Plasma::Label *m_title;
	Plasma::Label *m_courtesy;
	QGraphicsWidget *m_departureView;
	QList<Departure*> m_departures;

	// Data source info
	QDateTime m_lastSourceUpdate;
	QString m_sourceName;

	Plasma::Svg m_svg;
	QPointF m_timelineStart;
	QPointF m_timelineEnd;
	bool m_animate;
};

// This is the command that links your applet to the .desktop file
K_EXPORT_PLASMA_APPLET(graphicaltimetableline, GraphicalTimetableLine)
#endif
