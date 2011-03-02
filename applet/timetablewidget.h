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

#ifndef TIMETABLEWIDGET_H
#define TIMETABLEWIDGET_H

#include "departuremodel.h"

#include <Plasma/ScrollWidget>
#include <KDebug>

#include <QGraphicsWidget>
#include <QModelIndex>
#include <QPainter>
#include <QPointer>

namespace Plasma
{
	class Svg;
}

class QPropertyAnimation;
class QWidget;
class QTextLayout;
class QStyleOptionGraphicsItem;
class QPainter;

class RouteGraphicsItem;
class JourneyRouteGraphicsItem;
class DepartureModel; // TODO
class DepartureItem;
class TimetableWidget;
class PublicTransportWidget;

/** 
 * @brief Base class for DepartureGraphicsItem and JourneyGraphicsItem.
 **/
class PublicTransportGraphicsItem : public QGraphicsWidget {
	Q_OBJECT
	Q_PROPERTY( qreal expandStep READ expandStep WRITE setExpandStep )
	Q_PROPERTY( qreal fadeOut READ fadeOut WRITE setFadeOut )
	friend class PublicTransportWidget;
	
public:
    PublicTransportGraphicsItem( QGraphicsItem* parent = 0 );
    virtual ~PublicTransportGraphicsItem();
	
	static const qreal ROUTE_ITEM_HEIGHT = 60.0;
	
	PublicTransportWidget *publicTransportWidget() const { return m_parent; }; // TODO
	void setPublicTransportWidget( PublicTransportWidget* publicTransportWidget ) { 
		m_parent = publicTransportWidget;
	};
	QModelIndex index() const { return m_item->index(); };
	bool hasExtraIcon() const;
	
	int extraIconSize() const;
	
	qreal fadeOut() const { return m_fadeOut; };
	void setFadeOut( qreal fadeOut ) { m_fadeOut = fadeOut; updateGeometry(); };
	
	bool isExpanded() const { return m_expanded; };
	void toggleExpanded();
	void setExpanded( bool expand = true );
	
	virtual qreal unexpandedHeight() const;
	qreal expandStep() const { return m_expandStep; };
	void setExpandStep( qreal expandStep ) { m_expandStep = expandStep; updateGeometry(); };
	
	inline qreal padding() const;
	void capturePixmap();
	
	virtual qreal expandSize() const = 0;
	virtual void paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option, 
								const QRectF &rect ) = 0;
	
	virtual inline QColor textColor() const {
		#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
			return Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
		#else
			return Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewTextColor);
		#endif
	};
	virtual inline QColor backgroundColor() const {
		#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
			return Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor);
		#else
			return Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewBackgroundColor);
		#endif
	};
	
protected slots:
	void resizeAnimationFinished();
	
protected:
	virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
	virtual void mousePressEvent( QGraphicsSceneMouseEvent* event );
	virtual void mouseReleaseEvent( QGraphicsSceneMouseEvent* event );
	virtual void updateGeometry();
	virtual void updateTextLayouts() = 0;
	
	QPointer<TopLevelItem> m_item;
	PublicTransportWidget *m_parent;
	bool m_expanded;
	qreal m_expandStep;
	qreal m_fadeOut;
	QPropertyAnimation *m_resizeAnimation;
	QPixmap *m_pixmap;
};

class TextDocumentHelper {
public:
	static QTextDocument *createTextDocument( const QString &html, const QSizeF &size,
			const QTextOption &textOption, const QFont &font );
	static void drawTextDocument( QPainter *painter, const QStyleOptionGraphicsItem* option, 
								  QTextDocument *document, const QRect &textRect, bool drawHalos );
	static qreal textDocumentWidth( QTextDocument *document );
};

/** 
 * @brief A QGraphicsWidget representing a departure/arrival with public transport.
 **/
class DepartureGraphicsItem : public PublicTransportGraphicsItem {
	Q_OBJECT
	friend class TimetableWidget;
	
public:
	DepartureGraphicsItem( QGraphicsItem* parent = 0 );
	
	void updateData( DepartureItem* item, bool update = false );
	inline DepartureItem *departureItem() const { return qobject_cast<DepartureItem*>(m_item); };
	
	virtual QSizeF sizeHint( Qt::SizeHint which, const QSizeF& constraint = QSizeF() ) const;
	virtual qreal expandSize() const;
	
	virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option, 
						QWidget* widget = 0 );
	virtual void paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
								const QRectF &rect );
	
	QRect vehicleRect( const QRect &rect ) const;
	QRect infoRect( const QRect &rect, qreal timeColumnWidth ) const;
	QRect extraIconRect( const QRect &rect, qreal timeColumnWidth ) const;
	QRect timeRect( const QRect &rect ) const;
	
protected:
	virtual void updateTextLayouts();
	qreal timeColumnWidth() const;
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
	
private:
	QTextDocument *m_infoTextDocument;
	QTextDocument *m_timeTextDocument;
	RouteGraphicsItem *m_routeItem;
};

/** 
 * @brief A QGraphicsWidget representing a journey with public transport from A to B.
 **/
class JourneyGraphicsItem : public PublicTransportGraphicsItem {
	Q_OBJECT
	friend class TimetableWidget;
	
public:
	JourneyGraphicsItem( QGraphicsItem* parent = 0 );
	
	void updateData( JourneyItem* item, bool update = false );
	inline JourneyItem *journeyItem() const { return qobject_cast<JourneyItem*>(m_item); };
	
	virtual QSizeF sizeHint( Qt::SizeHint which, const QSizeF& constraint = QSizeF() ) const;
	virtual qreal expandSize() const;
	
	virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option, 
						QWidget* widget = 0 );
	virtual void paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
								const QRectF &rect );
	
	QRect vehicleRect( const QRect &rect ) const;
	QRect infoRect( const QRect &rect ) const;
	QRect extraIconRect( const QRect &rect ) const;
	
signals:
	void requestJourneys( const QString &startStop, const QString &targetStop );
	
protected:
	virtual void updateTextLayouts();
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
	
private:
	QTextDocument *m_infoTextDocument;
	JourneyRouteGraphicsItem *m_routeItem;
};

/** 
 * @brief Base class for TimetableWidget and JourneyTimetableWidget.
 **/
class PublicTransportWidget : public Plasma::ScrollWidget
{
	Q_OBJECT
	friend class PublicTransportGraphicsItem;
	friend class DepartureItem;
	
public:
	PublicTransportWidget( QGraphicsItem* parent = 0 );
	
	PublicTransportModel *model() const { return m_model; };
	void setModel( PublicTransportModel *model );
	PublicTransportGraphicsItem *item( int row ) { return m_items[row]; };
	
	void setSvg( Plasma::Svg *svg ) { m_svg = svg; };
	Plasma::Svg *svg() const { return m_svg; };
	
    void setIconSize( qreal iconSize ) { m_iconSize = iconSize; updateItemLayouts(); };
	qreal iconSize() const { return m_maxLineCount == 1 ? m_iconSize * m_zoomFactor * 0.75
			: m_iconSize * m_zoomFactor; };
	
	void setZoomFactor( qreal zoomFactor = 1.0 ) { m_zoomFactor = zoomFactor; };
	qreal zoomFactor() const { return m_zoomFactor; };
	
    void setNoItemsText( const QString &noItemsText ) {
		m_noItemsText = noItemsText;
		update();
	};
	QString noItemsText() const { return m_noItemsText; };
	
	void setMaxLineCount( int maxLineCount ) { m_maxLineCount = maxLineCount; updateItemGeometries(); };
    int maxLineCount() const { return m_maxLineCount; };
	
	/** Call this eg. when the DepartureArrivalListType changes in the model (only header data gets changed...). */
	void updateItemLayouts();
	
signals:
	void contextMenuRequested( PublicTransportGraphicsItem *item, const QPointF &pos );
	
protected slots:
	void journeysAboutToBeRemoved( const QList<ItemBase*> &journeys );
//     virtual void rowsInserted( const QModelIndex &parent, int first, int last );
    virtual void rowsRemoved( const QModelIndex &parent, int first, int last );
    virtual void modelReset();
    virtual void layoutChanged();
    virtual void dataChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight ) = 0;
	
protected:
    virtual QSizeF sizeHint( Qt::SizeHint which, const QSizeF& constraint ) const;
	virtual void contextMenuEvent( QGraphicsSceneContextMenuEvent* event );
    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option, 
						QWidget* widget = 0 );
	void updateItemGeometries();
	
	PublicTransportModel *m_model;
	QList<PublicTransportGraphicsItem*> m_items;
	Plasma::Svg *m_svg;
	qreal m_iconSize;
	qreal m_zoomFactor;
	int m_maxLineCount;
	QString m_noItemsText;
};

/** 
 * @brief A Plasma::ScrollWidget containing DepartureGraphicsItems.
 **/
class TimetableWidget : public PublicTransportWidget
{
	Q_OBJECT
	friend class PublicTransportGraphicsItem;
	friend class DepartureGraphicsItem;
	
public:
	TimetableWidget( QGraphicsItem* parent = 0 );
	
    void setTargetHidden( bool targetHidden ) { m_targetHidden = targetHidden; updateItemLayouts(); };
	bool isTargetHidden() const { return m_targetHidden; };
	
	inline DepartureGraphicsItem *departureItem( int row ) { 
		return qobject_cast<DepartureGraphicsItem*>(item(row));
	};
	inline DepartureModel *departureModel() const { 
		return qobject_cast<DepartureModel*>(m_model);
	};
	
protected slots:
    virtual void rowsInserted( const QModelIndex &parent, int first, int last );
    virtual void dataChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight );
	
private:
	bool m_targetHidden;
};

/** 
 * @brief A Plasma::ScrollWidget containing JourneyGraphicsItems.
 **/
class JourneyTimetableWidget : public PublicTransportWidget
{
	Q_OBJECT
	friend class PublicTransportGraphicsItem;
	
public:
	JourneyTimetableWidget( QGraphicsItem* parent = 0 );
	
	inline JourneyGraphicsItem *journeyItem( int row ) { 
		return qobject_cast<JourneyGraphicsItem*>(item(row));
	};
	inline JourneyModel *journeyModel() const { 
		return qobject_cast<JourneyModel*>(m_model);
	};
	
signals:
	void requestJourneys( const QString &startStop, const QString &targetStop );
	
protected slots:
    virtual void rowsInserted( const QModelIndex &parent, int first, int last );
    virtual void dataChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight );
};

#endif // TIMETABLEWIDGET_H
