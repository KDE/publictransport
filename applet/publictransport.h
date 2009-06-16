/*
 *   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef PUBLICTRANSPORT_HEADER
#define PUBLICTRANSPORT_HEADER

#include <KIcon>
#include <Plasma/PopupApplet>
#include <Plasma/Svg>
#include <Plasma/Label>
// #include <QTreeWidget>
#include <QtNetwork>
#include "ui_publicTransportConfig.h"
#include "ui_publicTransportFilterConfig.h"
#include <kconfigdialog.h>

class QSizeF;
class DepartureInfo;

// Shows departure times for public transport
class PublicTransport : public Plasma::PopupApplet
{
    Q_OBJECT
    public:
        // Basic Create/Destroy
        PublicTransport(QObject *parent, const QVariantList &args);
        ~PublicTransport();

        void paintInterface(QPainter *painter,
                const QStyleOptionGraphicsItem *option,
                const QRect& contentsRect);
	virtual QGraphicsWidget* graphicsWidget();
	virtual void constraintsEvent ( Plasma::Constraints constraints );
        void init();

	// The type of the vehicle used for a public transport line.
	// The numbers here must match the ones in the data engine!
	enum LineType
	{
	    Unknown = 0,
	    Tram = 1,
	    Bus = 2,
	    Subway = 3
	};

	// The type of services for a public transport line.
	enum LineService
	{
	    NoLineService = 0,

	    NightLine = 1,
	    ExpressLine = 2
	};
	Q_DECLARE_FLAGS( LineServices, LineService );
	
	enum FilterType
	{
	    ShowAll = 0,
	    ShowMatching = 1,
	    HideMatching = 2
	};

	struct DepartureInfo
	{
	    int lineNumber;
	    QString direction, lineString, duration;
	    QTime departure;
	    LineType lineType;
	    LineServices lineServices;

	    DepartureInfo( QString line, QString direction, QTime departure, LineType lineType )
	    {
		init( line, direction, departure, lineType, NoLineService );
	    }
	    
	    DepartureInfo( QString line, QString direction, QTime departure, LineType lineType, bool nightLine, bool expressLine )
	    {
		LineServices lineServices = NoLineService;
		if ( nightLine )
		    lineServices |= NightLine;
		if ( expressLine )
		    lineServices |= ExpressLine;
		init( line, direction, departure, lineType, lineServices );
	    }
	    
	    DepartureInfo( QString line, QString direction, QTime departure, LineType lineType, LineServices lineServices )
	    {
		init( line, direction, departure, lineType, lineServices );
	    }

	    void init( QString line, QString direction, QTime departure, LineType lineType, LineServices lineServices )
	    {
		QRegExp rx("[0-9]*");
		rx.lastIndexIn( line );
		if ( rx.isValid() )
		    this->lineNumber = rx.cap().toInt();
		else
		    this->lineNumber = -1;

		this->lineString = line;
		this->direction = direction;
		this->departure = departure;
		this->duration = getDurationString( QTime::currentTime().secsTo(departure) );
		this->lineType = lineType;
		this->lineServices = lineServices;
	    }
	    
	    QString getDurationString( int seconds ) const;
	    
	    bool isNightLine() const { return lineServices.testFlag( NightLine ); };
	    bool isExpressLine() const { return lineServices.testFlag( ExpressLine ); };
	};

    private:
	bool checkConfig();
	void generateInfoText();
	// Generates tooltip data and registers this applet at plasma's TooltipManager
	void createTooltip();
	void reconnectSource();
	
        Ui::publicTransportConfig m_ui;
        Ui::publicTransportFilterConfig m_uiFilter;

        Plasma::Svg m_svg;
        KIcon m_icon;
	QGraphicsWidget *m_graphicsWidget;
// 	QTreeWidget *m_treeWidget;
	Plasma::Label *m_label; // The main label of the plasmoid that shows HTML content (the timetable)
	
	QList<DepartureInfo> m_departureInfos; // List of current departures
	QString m_infoText; // HTML code containing the timetable
	QString m_currentSource;

	int m_updateTimeout; // A timeout to reload the timetable information from the internet
	bool m_showRemainingMinutes; // Wheather or not remaining minutes until departure should be shown
	bool m_showDepartureTime; // Wheather or not departure times should be shown
	QString m_city; // The currently selected city
	QString m_stop; // The currently selected stop
	int m_serviceProvider;
	
	bool	m_showTrams; // Wheather or not departures of trams should be shown
	bool	m_showBuses; // Wheather or not departures of buses should be shown
	bool	m_showNightlines; // Wheather or not night lines should be shown
	int m_filterMinLine; // The minimal line number to be shown
	int m_filterMaxLine; // The maximal line number to be shown
	FilterType m_filterTypeDirection; // The type of the filter (ShowAll, ShowMatching, HideMatching)
	QStringList m_filterDirectionList; // A list of directions that should be filtered

    signals:
	void settingsChanged();
	
    public slots:
	void configAccepted();
	void reload();
	void geometryChanged();
	void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );
	
    protected:
        void createConfigurationInterface(KConfigDialog *parent);
	virtual void popupEvent ( bool show );
};
 
// This is the command that links your applet to the .desktop file
K_EXPORT_PLASMA_APPLET(publictransport, PublicTransport)
#endif
