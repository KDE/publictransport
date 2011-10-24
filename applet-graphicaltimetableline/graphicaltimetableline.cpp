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

#include "graphicaltimetableline.h"

// libpublictransporthelper includes
#include <global.h>
#include <stopwidget.h>
#include <checkcombobox.h>
#include <vehicletypemodel.h>

// KDE includes
#include <KLocale>
#include <KConfigDialog>
#include <KStandardDirs>
#include <KToolInvocation>
#include "kdeversion.h"

// Plasma includes
#include <Plasma/Label>
#include <Plasma/ToolButton>
#include <Plasma/Svg>
#include <Plasma/Theme>
#include <Plasma/Animation>
#include <Plasma/PaintUtils>
#include <Plasma/ToolTipContent>
#include <Plasma/ToolTipManager>

// Qt includes
#include <qmath.h>
#include <QStyleOption>
#include <QFormLayout>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QTextLayout>
#include <QPainter>
#include <QFontMetrics>
#include <QSizeF>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QStandardItemModel>
#include <QGraphicsLinearLayout>
#include <QGraphicsGridLayout>
#include <QGraphicsSceneHoverEvent>

GraphicalTimetableLine::GraphicalTimetableLine(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args), m_stopWidget(0), m_vehicleTypeModel(0),
    m_showTimetableCheckbox(0), m_drawTransportLineCheckbox(0),
    m_zoomInButton(0), m_zoomOutButton(0), m_title(0), m_departureView(0), m_svg(this)
{
	m_animate = true;

    // this will get us the standard applet background, for free!
    setBackgroundHints(DefaultBackground);
    m_svg.setImagePath( KGlobal::dirs()->findResource("data", "plasma_applet_graphicaltimetableline/vehicles.svg") );
	m_svg.setContainsMultipleImages( true );

	setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    resize( 400, 250 );
	setMinimumHeight( 125 );

	QRectF rect = contentsRect();
	m_timelineStart = QPointF( rect.left() + 0.1 * rect.width(), rect.top() + 0.75 * rect.height() );
	m_timelineEnd = QPointF( rect.right() - 0.05 * rect.width(), rect.top() + 0.18 * rect.height() );
}


GraphicalTimetableLine::~GraphicalTimetableLine()
{
    if ( hasFailedToLaunch() ) {
        // Do some cleanup here
    } else {
        // Save settings
    }
}

void GraphicalTimetableLine::init()
{
    if ( !m_svg.hasElement("background") ) {
        setFailedToLaunch( true, i18n("No 'background' element found in the SVG") );
    }

	m_zoomInButton = new Plasma::ToolButton( this );
	m_zoomOutButton = new Plasma::ToolButton( this );
	m_zoomInButton->setIcon( KIcon("zoom-in") );
	m_zoomOutButton->setIcon( KIcon("zoom-out") );
	m_zoomInButton->setZValue( 999999 );
	m_zoomOutButton->setZValue( 999999 );
	connect( m_zoomInButton, SIGNAL(clicked()), this, SLOT(zoomIn()) );
	connect( m_zoomOutButton, SIGNAL(clicked()), this, SLOT(zoomOut()) );

	m_title = new Plasma::Label( this );
	QFont font = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
	font.setPixelSize( 14 );
	font.setBold( true );
	m_title->setFont( font );
	#if KDE_VERSION >= KDE_MAKE_VERSION(4,5,0)
		m_title->setWordWrap( false );
	#endif
	m_title->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	m_title->setZValue( 999999 );

	m_courtesy = new Plasma::Label;
	m_courtesy->setAlignment( Qt::AlignVCenter | Qt::AlignRight );
	connect( m_courtesy, SIGNAL(linkActivated(QString)),
				KToolInvocation::self(), SLOT(invokeBrowser(QString)) );
	QLabel *labelInfo = m_courtesy->nativeWidget();
	labelInfo->setOpenExternalLinks( true );
	labelInfo->setWordWrap( true );
	m_courtesy->setText( courtesyText() );
	m_courtesy->setZValue( 999999 );

	m_departureView = new QGraphicsWidget( this );
	m_departureView->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	m_departureView->translate( 0, -m_title->size().height() - 25 );

	QGraphicsGridLayout *l = new QGraphicsGridLayout( this );
	l->addItem( m_zoomInButton, 0, 0 );
	l->addItem( m_zoomOutButton, 0, 1 );
	l->addItem( m_title, 0, 2 );
	l->addItem( m_departureView, 1, 0, 1, 3 );
	l->addItem( m_courtesy, 2, 0, 1, 3 );

	m_stopSettings.set( ServiceProviderSetting,
						config().readEntry(QLatin1String("serviceProvider"), QString()) );
	m_stopSettings.set( StopNameSetting,
						config().readEntry(QLatin1String("stopName"), QString()) );

	m_timelineLength = config().readEntry( QLatin1String("timelineLength"), 10 );
	m_showTimetable = config().readEntry( QLatin1String("showTimetable"), true );
	m_drawTransportLine = config().readEntry( QLatin1String("drawTransportLine"), true );

	QVariantList vehicleTypes = config().readEntry( QLatin1String("vehicleTypes"), QVariantList() );
	if ( vehicleTypes.isEmpty() ) {
		m_vehicleTypes << Unknown << Tram << Bus << TrolleyBus << InterurbanTrain << Subway
				<< Metro << RegionalTrain << RegionalExpressTrain << InterregionalTrain
				<< IntercityTrain << HighSpeedTrain << Ship << Plane << Feet;
	} else {
		foreach ( const QVariant &vehicleType, vehicleTypes ) {
			m_vehicleTypes << static_cast<VehicleType>( vehicleType.toInt() );
		}
	}

	if ( m_stopSettings.stopList().isEmpty() ) {
		setConfigurationRequired( true, i18n("Please select a stop name") );
	} else if ( m_stopSettings.get<QString>(ServiceProviderSetting).isEmpty() ) {
		setConfigurationRequired( true, i18n("Please select a service provider") );
	} else {
		setConfigurationRequired( false );
	}

	if ( !configurationRequired() ) {
		m_sourceName = QString("Departures %1|stop=%2|timeOffset=0")
					.arg(m_stopSettings.get<QString>(ServiceProviderSetting))
					.arg(m_stopSettings.stop(0).nameOrId());
		dataEngine("publictransport")->connectSource( m_sourceName,
				this, 60000, Plasma::AlignToMinute );
	}

	createTooltip();
}

void GraphicalTimetableLine::createConfigurationInterface(KConfigDialog* parent)
{
	QWidget *stopConfig = new QWidget( parent );
	QFormLayout *stopLayout = new QFormLayout( stopConfig );

	m_stopWidget = new StopWidget( stopConfig, m_stopSettings );
	CheckCombobox *filterList = new CheckCombobox( stopConfig );
	m_vehicleTypeModel = new VehicleTypeModel( filterList );
	m_vehicleTypeModel->checkVehicleTypes( m_vehicleTypes );
	filterList->setModel( m_vehicleTypeModel );
	filterList->setAllowNoCheckedItem( false );
	m_showTimetableCheckbox = new QCheckBox( i18n("Enable"), stopConfig );
	m_drawTransportLineCheckbox = new QCheckBox( i18n("Enable"), stopConfig );
	m_showTimetableCheckbox->setChecked( m_showTimetable );
	m_drawTransportLineCheckbox->setChecked( m_drawTransportLine );
	m_drawTransportLineCheckbox->setToolTip( i18n("Draws the transport line string into the "
			"vehicle type icon, for icons that are associated with a single departure.") );

	stopLayout->addRow( m_stopWidget );
	stopLayout->addRow( i18n("Shown &Vehicles:"), filterList );
	stopLayout->addRow( i18n("Show &Timetable:"), m_showTimetableCheckbox );
	stopLayout->addRow( i18n("Draw Transport &Line:"), m_drawTransportLineCheckbox );
	parent->addPage( stopConfig, i18n("Stop") );

	connect( parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()) );
	connect( parent, SIGNAL(okClicked()), this, SLOT(configAccepted()) );

	m_stopWidget->setFocus();
	if ( m_stopSettings.stopList().isEmpty() || m_stopSettings.stop(0).name.isEmpty() ) {
		m_stopWidget->editSettings();
	}
}

void GraphicalTimetableLine::configAccepted()
{
	m_stopSettings = m_stopWidget->stopSettings();
	m_vehicleTypes = m_vehicleTypeModel->checkedVehicleTypes();

	qDeleteAll( m_departures );
	m_departures.clear();

	if ( m_stopSettings.stops().isEmpty() ) {
		setConfigurationRequired( true, i18n("Please select a stop name") );
	} else if ( m_stopSettings.get<QString>(ServiceProviderSetting).isEmpty() ) {
		setConfigurationRequired( true, i18n("Please select a service provider") );
	} else {
		setConfigurationRequired( false );
	}

	// Disconnect old source
	if ( !m_sourceName.isEmpty() ) {
		dataEngine("publictransport")->disconnectSource( m_sourceName, this );
	}

	if ( !configurationRequired() ) {
		m_animate = false;
		m_sourceName = QString("Departures %1|stop=%2|timeOffset=0")
					.arg(m_stopSettings.get<QString>(ServiceProviderSetting))
					.arg(m_stopSettings.stops(StopSettings::UseStopIdIfAvailable).first());
		dataEngine("publictransport")->connectSource( m_sourceName,
				this, 60000, Plasma::AlignToMinute );
	}

	m_showTimetable = m_showTimetableCheckbox->isChecked();
	m_drawTransportLine = m_drawTransportLineCheckbox->isChecked();

	config().writeEntry( QLatin1String("serviceProvider"),
						 m_stopSettings.get<QString>(ServiceProviderSetting) );
	config().writeEntry( QLatin1String("stopName"),
						 m_stopSettings.stops(StopSettings::UseStopIdIfAvailable).first() );

	config().writeEntry( QLatin1String("timelineLength"), m_timelineLength );
	config().writeEntry( QLatin1String("showTimetable"), m_showTimetable );
	config().writeEntry( QLatin1String("drawTransportLine"), m_drawTransportLine );

	QVariantList vehicleTypes;
	foreach ( VehicleType vehicleType, m_vehicleTypes ) {
		vehicleTypes << static_cast<int>( vehicleType );
	}
	config().writeEntry( QLatin1String("vehicleTypes"), vehicleTypes );

	emit configNeedsSaving();
	configChanged();

	m_stopWidget = 0;
	m_vehicleTypeModel = 0;
	m_showTimetableCheckbox = 0;
	m_drawTransportLineCheckbox = 0;
}

void GraphicalTimetableLine::createTooltip( Departure *departure )
{
	if ( isPopupShowing() || (formFactor() != Plasma::Horizontal
						  && formFactor() != Plasma::Vertical) ) {
		return;
	}

		kDebug() << "CREATE THE TOOLTIP" << departure;

	Plasma::ToolTipContent tooltipData;
	tooltipData.setMainText( i18nc("@info", "Public Transport") );
	if ( m_departures.isEmpty() ) {
		tooltipData.setSubText( i18nc("@info", "View departures for public transport") );
	} else {
		QList<DepartureData> dataList = departure ? departure->departureData()
				: m_departures.first()->departureData();
		if ( dataList.count() == 1 ) {
			// Signle departure item hovered
			DepartureData data = dataList.first();
			tooltipData.setSubText( i18n("Line %1 (%2) %3",
					data.transportLine, data.target, KGlobal::locale()->formatTime(data.time.time())) );
		} else {
			// Multiple departure items hovered
			QString text;
			foreach ( const DepartureData &data, dataList ) {
				text.append( i18n("Line %1 (%2) %3\n",
						data.transportLine, data.target, KGlobal::locale()->formatTime(data.time.time())) );
			}
			if ( text.endsWith('\n') ) {
				text.remove( text.length() - 1, 1 );
			}
			tooltipData.setSubText( text );
		}
	}

	tooltipData.setImage( KIcon("public-transport-stop").pixmap(IconSize(KIconLoader::Desktop)) );
	Plasma::ToolTipManager::self()->setContent( this, tooltipData );
}

QString GraphicalTimetableLine::courtesyText()
{
	QVariantHash data = dataEngine("publictransport")->query(
			QString("ServiceProvider %1").arg(m_stopSettings.get<QString>(ServiceProviderSetting)) );
	QString shortUrl = data[ "shortUrl" ].toString();
	QString url = data[ "url" ].toString();
	QString sLastUpdate = m_lastSourceUpdate.toString( "hh:mm" );
	if ( sLastUpdate.isEmpty() ) {
		sLastUpdate = i18nc("@info/plain This is used as 'last data update' text when there "
							"hasn't been any updates yet.", "none");
	}

	// HACK: This breaks the text at one position if needed
	// Plasma::Label doesn't work well will HTML formatted text and word wrap:
	// It sets the height as if the label shows the HTML source.
	QString textNoHtml1 = QString( "%1: %2" ).arg( i18nc( "@info/plain", "last update" ), sLastUpdate );
	QString textNoHtml2 = QString( "%1: %2" ).arg( i18nc( "@info/plain", "data by" ), shortUrl );
	QFontMetrics fm( m_courtesy->font() );
	int width1 = fm.width( textNoHtml1 );
	int width2 = fm.width( textNoHtml2 );
	int width = width1 + fm.width( ", " ) + width2;
	if ( width > size().width() ) {
		setMinimumWidth( qMax(150, qMax(width1, width2)) );
		return QString( "<nobr>%1: %2<br>%3: <a href='%4'>%5</a><nobr>" )
		       .arg( i18nc( "@info/plain", "last update" ), sLastUpdate,
		             i18nc( "@info/plain", "data by" ), url, shortUrl );
	} else {
		return QString( "<nobr>%1: %2, %3: <a href='%4'>%5</a><nobr>" )
		       .arg( i18nc( "@info/plain", "last update" ), sLastUpdate,
		             i18nc( "@info/plain", "data by" ), url, shortUrl );
	}
}

void GraphicalTimetableLine::zoomIn()
{
	m_timelineLength /= 1.5;
	if ( m_timelineLength <= MIN_TIMELINE_LENGTH ) {
		m_timelineLength = MIN_TIMELINE_LENGTH; // Minimally MIN_TIMELINE_LENGTH minutes
		m_zoomInButton->setEnabled( false );
	}
	m_zoomOutButton->setEnabled( true );
	updateItemPositions( false );
	updateTitle();
	update();
}

void GraphicalTimetableLine::zoomOut()
{
	m_timelineLength *= 1.5;
	if ( m_timelineLength >= MAX_TIMELINE_LENGTH ) {
		m_timelineLength = MAX_TIMELINE_LENGTH; // Maximally MAX_TIMELINE_LENGTH minutes
		m_zoomOutButton->setEnabled( false );
	}
	m_zoomInButton->setEnabled( true );
	updateItemPositions( false );
	updateTitle();
	update();
}

QDateTime GraphicalTimetableLine::endTime() const
{
	return QDateTime::currentDateTime().addSecs( 60 * m_timelineLength );
}

void GraphicalTimetableLine::dataUpdated(const QString& sourceName,
										 const Plasma::DataEngine::Data& data)
{
	Q_UNUSED( sourceName );

	kDebug() << m_departures.count() << "departures at start";
	for ( int i = m_departures.count() - 1; i >= 0; --i ) {
		Departure *departure = m_departures[i];
		if ( QDateTime::currentDateTime().secsTo(departure->dateTime()) <= -40
			&& departure->isVisible() )
		{
			// Remove old departure
			kDebug() << "Animate out old departure" << i << "at" << departure->dateTime().time().toString();
			m_departures.removeAt( i );

			departure->setZValue( 999999 );

			QPropertyAnimation *rotateAnimation =
					new QPropertyAnimation( departure, "rotation", this );
			rotateAnimation->setStartValue( 0.0 );
			rotateAnimation->setEndValue( 360.0 );
			rotateAnimation->setEasingCurve( QEasingCurve(QEasingCurve::OutInBack) );
			rotateAnimation->setDuration( 1000 );

			Plasma::Animation *slideAnimation =
					Plasma::Animator::create( Plasma::Animator::SlideAnimation, departure );
			slideAnimation->setEasingCurve( QEasingCurve(QEasingCurve::InCubic) );
			slideAnimation->setTargetWidget( departure );
			slideAnimation->setProperty( "movementDirection", Plasma::Animation::MoveRight );
			slideAnimation->setProperty( "distance", contentsRect().width() - departure->boundingRect().right() );
			slideAnimation->setProperty( "duration", 750 );

			Plasma::Animation *fadeAnimation =
					Plasma::Animator::create( Plasma::Animator::FadeAnimation, departure );
			fadeAnimation->setTargetWidget( departure );
			fadeAnimation->setProperty( "startOpacity", 1.0 );
			fadeAnimation->setProperty( "targetOpacity", 0.0 );
			fadeAnimation->setProperty( "duration", 750 );

			QParallelAnimationGroup *parallelGroup = new QParallelAnimationGroup( departure );
			parallelGroup->addAnimation( slideAnimation );
			parallelGroup->addAnimation( fadeAnimation );

			QSequentialAnimationGroup *sequentialGroup = new QSequentialAnimationGroup( departure );
			sequentialGroup->addAnimation( rotateAnimation );
			sequentialGroup->addAnimation( parallelGroup );
			connect( sequentialGroup, SIGNAL(finished()), departure, SLOT(deleteLater()) );
			sequentialGroup->start( QAbstractAnimation::DeleteWhenStopped );
		}
	}

	QUrl url;
	QDateTime updated;
	url = data["requestUrl"].toUrl();
	updated = data["updated"].toDateTime();
	int count = data["count"].toInt();
// 	int count = qMin( 10, data["count"].toInt() );
	kDebug() << "  - " << count << "departures to be processed";
	for ( int i = 0; i < count; ++i ) {
		QVariant dataItem = data.value( QString::number(i) );
		// Don't process invalid data
		if ( !dataItem.isValid() ) {
			kDebug() << "Departure data for departure" << i << "is invalid" << data;
			continue;
		}

		QVariantHash dataMap = dataItem.toHash();
		VehicleType vehicleType = static_cast<VehicleType>( dataMap["vehicleType"].toInt() );
		if ( !m_vehicleTypes.contains(vehicleType) ) {
			continue; // Fitlered
		}

		QDateTime dateTime = dataMap["departure"].toDateTime();
		if ( QDateTime::currentDateTime().secsTo(dateTime) < -60 ) {
			kDebug() << "Got an old departure" << dateTime;
			continue;
		}

		DepartureData departureData( dateTime, dataMap["line"].toString(),
									 dataMap["target"].toString(), vehicleType );

		bool departureIsOld = false;
		foreach ( Departure *departure, m_departures ) {
			if ( departure->containsDeparture(departureData) ) {
				departureIsOld = true;
				break;
			}
		}
		if ( departureIsOld ) {
			continue; // Departure was already added in a previous dataUpdated call
		}

		Departure *departure = new Departure( m_departureView, departureData );
		departure->setPos( m_timelineEnd );
		m_departures << departure;
	} // for ( int i = 0; i < count; ++i )

	// Update "last update" time
	if ( updated > m_lastSourceUpdate ) {
		m_lastSourceUpdate = updated;
	}

	updateTitle();
	m_courtesy->setText( courtesyText() );
	createTooltip();

	kDebug() << m_departures.count() << "departures after adding new ones";
	updateItemPositions( m_animate );
	m_animate = true;
	update();
}

void GraphicalTimetableLine::updateTitle()
{
	if ( !m_title || m_stopSettings.stopList().isEmpty() ) {
		return;
	}
	QFontMetrics fm( m_title->font() );
	qreal maxStopNameWidth = contentsRect().width() - m_zoomOutButton->boundingRect().right() - 50
			- fm.width(" (99:99 - 99:99)");
	m_title->setText( QString("%1 (%2 - %3)")
			.arg( fm.elidedText(m_stopSettings.stop(0), Qt::ElideRight, maxStopNameWidth) )
			.arg( KGlobal::locale()->formatTime(QTime::currentTime()) )
			.arg( KGlobal::locale()->formatTime(endTime().time()) ) );
}

Departure::Departure( QGraphicsItem* parent, const DepartureData &data, const QPointF &pos )
		: QGraphicsWidget(parent)
{
	m_size = QSizeF( DEPARTURE_SIZE, DEPARTURE_SIZE );

	QFont f = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
	f.setBold( true );
	f.setPixelSize( 13 );
	setFont( f );

	m_departures << data;
	setPos( pos );
	updatePosition();
	updateDrawData();
	updateTooltip();
}

Departure::Departure(QGraphicsItem* parent, const QList< DepartureData >& dataList,
					 const QPointF &pos)
		: QGraphicsWidget(parent)
{
	m_size = QSizeF( DEPARTURE_SIZE, DEPARTURE_SIZE );

	QFont f = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
	f.setBold( true );
	f.setPixelSize( 13 );
	setFont( f );

	m_departures = dataList;
	setPos( pos );
	updatePosition();
	updateDrawData();
	updateTooltip();
}

QPointF Departure::updatePosition( bool animate )
{
	GraphicalTimetableLine *applet = qobject_cast<GraphicalTimetableLine*>(parentWidget()->parentWidget());
	qreal newOpacity, zoom, z;
	QPointF position = applet->positionFromTime( m_departures.first().time, &newOpacity, &zoom, &z );
	if ( position.isNull() ) {
// 		hide();
		if ( pos().isNull() ) {
			setOpacity( 0.0 );
		} else  if ( isVisible() && opacity() > 0.0 ) {
			Plasma::Animation *fadeAnimation =
					Plasma::Animator::create( Plasma::Animator::FadeAnimation, this );
			fadeAnimation->setTargetWidget( this );
			fadeAnimation->setProperty( "startOpacity", opacity() );
			fadeAnimation->setProperty( "targetOpacity", 0.0 );
			fadeAnimation->start( QAbstractAnimation::DeleteWhenStopped );
		}
	} else {
		int msecs = animate ? 5000 : 250;

		if ( pos().isNull() ) {
// 			show();
			setPos( applet->newDeparturePosition() );
// 			setPos( position );
			setZValue( z );
			setSize( QSizeF(DEPARTURE_SIZE * zoom, DEPARTURE_SIZE * zoom) );

// 			Plasma::Animation *fadeAnimation =
// 					Plasma::Animator::create( Plasma::Animator::FadeAnimation, this );
// 			fadeAnimation->setTargetWidget( this );
// 			fadeAnimation->setProperty( "startOpacity", 0.0 );
// 			fadeAnimation->setProperty( "targetOpacity", newOpacity );
// 			fadeAnimation->start( QAbstractAnimation::DeleteWhenStopped );
		}
// 		else {
			QPropertyAnimation *moveAnimation = new QPropertyAnimation( this, "pos" );
			moveAnimation->setDuration( msecs );
			moveAnimation->setEasingCurve( QEasingCurve(QEasingCurve::InOutQuad) );
			moveAnimation->setStartValue( pos() );
			moveAnimation->setEndValue( position );
// 			moveAnimation->start( QAbstractAnimation::DeleteWhenStopped );
// 		}

		Plasma::Animation *fadeAnimation = NULL;
		if ( opacity() != newOpacity ) {
			fadeAnimation = Plasma::Animator::create( Plasma::Animator::FadeAnimation, this );
			fadeAnimation->setTargetWidget( this );
			fadeAnimation->setProperty( "duration", msecs );
			fadeAnimation->setProperty( "startOpacity", opacity() );
			fadeAnimation->setProperty( "targetOpacity", newOpacity );
		}
		QPropertyAnimation *zoomAnimation = new QPropertyAnimation( this, "size" );
		zoomAnimation->setDuration( msecs );
		zoomAnimation->setStartValue( m_size );
		zoomAnimation->setEndValue( QSizeF(DEPARTURE_SIZE * zoom, DEPARTURE_SIZE * zoom) );

		QParallelAnimationGroup *parallelGroup = new QParallelAnimationGroup( this );
		if ( fadeAnimation ) {
			parallelGroup->addAnimation( fadeAnimation );
		}
		parallelGroup->addAnimation( zoomAnimation );
		parallelGroup->addAnimation( moveAnimation );
		parallelGroup->start( QAbstractAnimation::DeleteWhenStopped );
// 			} else {
// 				setOpacity( newOpacity );
// 				m_size = QSizeF( DEPARTURE_SIZE * zoom, DEPARTURE_SIZE * zoom );
// 			}

		setZValue( z );
	}

	return position;
}

void Departure::updateTooltip()
{
	QString text = i18np("<b>One Departure:</b>", "<b>%1 Departures:</b>", m_departures.count());
	text.append( "<br />" );
	// Show maximally 10 departures
	for ( int i = 0; i < qMin(10, m_departures.count()); ++i ) {
		const DepartureData data = m_departures[i];
		// TODO KUIT
		text.append( i18n("Line <b>%1</b> at <b>%2</b> to %3", data.transportLine,
						  KGlobal::locale()->formatTime(data.time.time()), data.target) );
		text.append( "<br />" );
	}
	if ( m_departures.count() > 10 ) {
		text.append( i18np("<i>...one more departure</i>", "<i>...%1 more departures</i>", m_departures.count() - 10) );
	}
	if ( text.endsWith(QLatin1String("<br />")) ) {
		text.remove( text.length() - 6, 6 );
	}

	setToolTip( text );
}

void Departure::updateDrawData()
{
	m_drawData.clear();
	QSet<VehicleType> drawnVehicleTypes;
	QSet<VehicleType> doubleVehicleTypes;
	QSet<VehicleType> dontDrawTransportLineVehicleTypes;
	for ( int i = 0; i < m_departures.count(); ++i ) {
		DepartureData &data = m_departures[i];
		if ( drawnVehicleTypes.contains(data.vehicleType) ) {
			// There is already a departure drawn with this vehicle type
			if ( doubleVehicleTypes.contains(data.vehicleType) ) {
				if ( !dontDrawTransportLineVehicleTypes.contains(data.vehicleType) ) {
					// Don't draw the transport line string for vehicles types that have
					// more than two associated departures in this departure item
					dontDrawTransportLineVehicleTypes << data.vehicleType;

					for ( int i = m_departures.count() - 1; i >= 0; --i ) {
						if ( dontDrawTransportLineVehicleTypes.contains(m_departures[i].vehicleType) ) {
							m_departures[i].drawTransportLine = false;
						}
					}
				}
			} else {
				// There is only one other departure with this vehicle type
				data.drawTransportLine = true;
				m_drawData << &data;
				doubleVehicleTypes << data.vehicleType;
			}
		} else {
			// First departure with this vehicle type
			data.drawTransportLine = true;
			m_drawData << &data;
			drawnVehicleTypes << data.vehicleType;
		}
	}

// 	kDebug() << m_departures.count() << ", drawn:" << m_drawData.count();

	// Don't draw double vehicle types if there are more than four items to be drawn
	if ( m_drawData.count() > 4 ) {
		for ( int i = m_drawData.count() - 1; i >= 0; --i ) {
			if ( doubleVehicleTypes.contains(m_drawData[i]->vehicleType) ) {
				doubleVehicleTypes.remove( m_drawData[i]->vehicleType );
				m_drawData.removeAt( i );
			}
		}
	}
// 	kDebug() << "After removing:" << m_departures.count() << ", drawn:" << m_drawData.count();
}

void Departure::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
	Q_UNUSED( option );
	Q_UNUSED( widget );

    painter->setRenderHint( QPainter::SmoothPixmapTransform );
    painter->setRenderHint( QPainter::Antialiasing );

	GraphicalTimetableLine *applet = qobject_cast<GraphicalTimetableLine*>(parentWidget()->parentWidget());
	QRectF rect = boundingRect();

	QRectF vehicleRect = rect;
	qreal factor = departureSizeFactor();
	vehicleRect.setWidth( vehicleRect.width() * factor );
	vehicleRect.setHeight( vehicleRect.height() * factor );
	qreal translation = departureOffset( vehicleRect.width() );
	foreach ( const DepartureData *data, m_drawData ) {
		if ( data->drawTransportLine ) {
			applet->paintVehicle( painter, data->vehicleType, vehicleRect, data->transportLine );
		} else {
			applet->paintVehicle( painter, data->vehicleType, vehicleRect );
		}

		// Move to next vehicle type svg position
		vehicleRect.translate( translation, translation );
	}

	QDateTime minTime = m_departures.first().time, maxTime = minTime;
	foreach ( const DepartureData &data, m_departures ) {
		if ( data.time < minTime ) {
			minTime = data.time;
		} else if ( data.time > maxTime ) {
			maxTime = data.time;
		}
	}
	QString text;
	if ( m_departures.count() != m_drawData.count() ) {
		// Show the number of departures if there are more than vehicle icons drawn
		text.append( QString("%1: ").arg(m_departures.count()) );
	}

	QDateTime currentTime = QDateTime::currentDateTime();
	if ( minTime == maxTime ) {
		int minsToDeparture = qCeil( currentTime.secsTo(minTime) / 60.0 );
		if ( minsToDeparture < 0 ) {
			text.append( i18n("leaving") );
		} else if ( minsToDeparture == 0 ) {
			text.append( i18n("now") );
		} else {
			text.append( i18np("in 1 min.", "in %1 min.", minsToDeparture) );
		}
	} else {
		int minMinsToDeparture = qCeil( currentTime.secsTo(minTime) / 60.0 );
		int maxMinsToDeparture = qCeil( currentTime.secsTo(maxTime) / 60.0 );
		if ( minMinsToDeparture < 0 && maxMinsToDeparture < 0 ) {
			text.append( i18n("leaving") );
		} else if ( minMinsToDeparture == 0 && maxMinsToDeparture == 0 ) {
			text.append( i18n("now") );
		} else {
			text.append( i18n("in %1-%2 min.", minMinsToDeparture, maxMinsToDeparture) );
		}
	}

	QFontMetrics fm( font() );
	int textWidth = fm.width( text );
	QRectF textRect = rect;
	QRectF haloRect( textRect.left() + (textRect.width() - textWidth) / 2,
					 textRect.top() + (textRect.height() - fm.height()) / 2,
					 textWidth, fm.height() );
	haloRect = haloRect.intersected( textRect );

	painter->rotate( 45 ); // Draw the text 45 degree rotated, just along the arrangement of vehicle type icons
	Plasma::PaintUtils::drawHalo( painter, haloRect );
	painter->setFont( font() );
	painter->drawText( textRect, text, QTextOption(Qt::AlignCenter) );
}

QSizeF Departure::sizeHint( Qt::SizeHint which, const QSizeF& constraint ) const
{
	Q_UNUSED( which );
	Q_UNUSED( constraint );
    return m_size;
}

QRectF Departure::boundingRect() const
{
	qreal padding = 20;
    return QRectF( -m_size.width() / 2.0 - padding, -m_size.height() / 2.0 - padding,
				   m_size.width() + 2 * padding, m_size.height() + 2 * padding );
}

void Departure::combineWith( Departure *other )
{
	m_departures << other->departureData();
	updateDrawData();
	updateTooltip();
}

Departure* Departure::splitAt( QGraphicsItem *parent, int index )
{
	if ( m_departures.count() == 1 || index == 0 ) {
		// Departure items should at least contain one departure
		return NULL;
	}

	// Create new Departure item with departures beginning with index
	Departure *departure = new Departure( parent, m_departures.mid(index), pos() );

	// Remove departures that moved into the new Departure item
	while ( m_departures.count() > index ) {
		m_departures.removeLast();
	}

	updateDrawData();
	updateTooltip();
	return departure;
}

void Departure::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
	GraphicalTimetableLine *applet = qobject_cast<GraphicalTimetableLine*>(parentWidget()->parentWidget());
    QGraphicsItem::hoverEnterEvent(event);

	kDebug() << "Create the departure tooltip" << this;
	applet->createTooltip( this );
}

QPointF GraphicalTimetableLine::positionFromTime( const QDateTime& time, qreal *opacity,
												  qreal *zoom, qreal *z ) const
{
	qreal minutesToDeparture = qCeil( QDateTime::currentDateTime().secsTo(time) / 60.0 );
	if ( minutesToDeparture > m_timelineLength || minutesToDeparture < 0 ) {
		return QPointF();
	}

	qreal position = minutesToDeparture / m_timelineLength; // 0 .. 1

	if ( opacity ) {
		*opacity = position < 0.5 ? 1.0 : 1.0 - 2.0 * (position - 0.5);
	}
	if ( zoom ) {
		*zoom = 1.5 * (2.0 - position);
	}
	if ( z ) {
		*z = 1.0 - position;
	}
	return QPointF( m_timelineStart.x() + position * (m_timelineEnd.x() - m_timelineStart.x()),
					m_timelineStart.y() + position * (m_timelineEnd.y() - m_timelineStart.y()) );
}

void GraphicalTimetableLine::updateItemPositions( bool animate )
{
	QPointF lastPos;
	Departure *lastDeparture = NULL;
	for ( int i = 0; i < m_departures.count(); ++i ) {
		Departure *departure = m_departures[i];
		QPointF newPos = departure->updatePosition( animate );

		// Split departure items eg. after zooming out
		QList<DepartureData> departureData = departure->departureData();
		QPointF lastSubPos = positionFromTime(departureData.first().time);
		for ( int n = 1; n < departureData.count(); ++n ) {
			QPointF subPos = positionFromTime(departureData[n].time);
			if ( (lastSubPos - subPos).manhattanLength() > MIN_DISTANCE_BETWEEN_DEPARTURES ) {
				// Departure isn't too close to the last departure
				// Split them to two objects
				Departure *splitDeparture = departure->splitAt( m_departureView, n );
				if ( splitDeparture ) {
					m_departures.insert( i + 1, splitDeparture );
					splitDeparture->updatePosition( animate );
				}
				break;
			}
			lastSubPos = subPos;
		}

		if ( lastDeparture && (lastPos - newPos).manhattanLength() < MIN_DISTANCE_BETWEEN_DEPARTURES ) {
			// Departure is very close to the last departure
			// Combine both into one object
			lastDeparture->combineWith( departure );

			// Delete combined departure item
			m_departures.removeAt( i );
			delete departure;
			--i;
		} else if ( !newPos.isNull() ) {
			lastDeparture = departure;
			lastPos = newPos;
		}
	}
	return;
}

void GraphicalTimetableLine::paintVehicle( QPainter* painter, VehicleType vehicle,
										   const QRectF& rect, const QString &transportLine )
{
	// Draw transport line string onto the vehicle type svg
	// only if activated in the settings and a supported vehicle type
	// (currently only local public transport)
	bool drawTransportLine = m_drawTransportLine && !transportLine.isEmpty()
			&& Timetable::Global::generalVehicleType(vehicle) == LocalPublicTransport;

	QString vehicleKey;
	switch ( vehicle ) {
		case Tram: vehicleKey = "tram"; break;
		case Bus: vehicleKey = "bus"; break;
		case TrolleyBus: vehicleKey = "trolleybus"; break;
		case Subway: vehicleKey = "subway"; break;
		case Metro: vehicleKey = "metro"; break;
		case InterurbanTrain: vehicleKey = "interurbantrain"; break;
		case RegionalTrain: vehicleKey = "regionaltrain"; break;
		case RegionalExpressTrain: vehicleKey = "regionalexpresstrain"; break;
		case InterregionalTrain: vehicleKey = "interregionaltrain"; break;
		case IntercityTrain: vehicleKey = "intercitytrain"; break;
		case HighSpeedTrain: vehicleKey = "highspeedtrain"; break;
		case Feet: vehicleKey = "feet"; break;
		case Ship: vehicleKey = "ship"; break;
		case Plane: vehicleKey = "plane"; break;
		default:
			kDebug() << "Unknown vehicle type" << vehicle;
			return; // TODO: draw a simple circle or something.. or an unknown vehicle type icon
	}
	if ( drawTransportLine ) {
		vehicleKey.append( "_empty" );
	}
	if ( !m_svg.hasElement(vehicleKey) ) {
		kDebug() << "SVG element" << vehicleKey << "not found";
		return;
	}

	int shadowWidth = 4;
    m_svg.resize( rect.width() - 2 * shadowWidth, rect.height() - 2 * shadowWidth );

	QPixmap pixmap( (int)rect.width(), (int)rect.height() );
	pixmap.fill( Qt::transparent );
	QPainter p( &pixmap );
    m_svg.paint( &p, shadowWidth, shadowWidth, vehicleKey );

	// Draw transport line string (only for local public transport)
	if ( drawTransportLine ) {
		QString text = transportLine;
		text.remove( ' ' );

		QFont f = font();
		f.setBold( true );
		if ( text.length() > 2 ) {
			f.setPixelSize( qMax(8, qCeil(1.2 * rect.width() / text.length())) );
		} else {
			f.setPixelSize( rect.width() * 0.55 );
		}
		p.setFont( f );
		p.setPen( Qt::white );

		QRect textRect( shadowWidth, shadowWidth,
						rect.width() - 2 * shadowWidth, rect.height() - 2 * shadowWidth );
		p.drawText( textRect, text, QTextOption(Qt::AlignCenter) );
	}

	QImage shadow = pixmap.toImage();
	Plasma::PaintUtils::shadowBlur( shadow, shadowWidth - 1, Qt::black );
	painter->drawImage( rect.topLeft() + QPoint(1, 2), shadow );
	painter->drawPixmap( rect.topLeft(), pixmap );
}

void GraphicalTimetableLine::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    Plasma::Applet::resizeEvent(event);

	QRectF rect = contentsRect();
	m_timelineStart = QPointF( rect.left() + 0.1 * rect.width(), rect.top() + 0.75 * rect.height() );
	m_timelineEnd = QPointF( rect.right() - 0.05 * rect.width(), rect.top() + 0.18 * rect.height() );

	qreal scale = qMax( qreal(0.4), qMin(qreal(1.0),
                                         qreal(qMin(rect.width(), rect.height()) / 250)) );
	foreach ( Departure *departure, m_departures ) {
		departure->setScale( scale );
	}
	updateItemPositions( false );
	updateTitle(); // New eliding
}

void GraphicalTimetableLine::paintInterface( QPainter *p,
        const QStyleOptionGraphicsItem *option, const QRect &rect )
{
	Q_UNUSED( option );
    p->setRenderHint( QPainter::SmoothPixmapTransform );
    p->setRenderHint( QPainter::Antialiasing );

	// Draw background
	if ( !m_svg.hasElement("background") ) {
		kDebug() << "Background SVG element not found";
		return;
	}
    m_svg.resize( (int)rect.width(), (int)rect.height() );
    m_svg.paint( p, rect, "background" );

	// Draw text markers (every full hour)
	QFont timeMarkerFont = font();
	timeMarkerFont.setBold( true );
	p->setFont( timeMarkerFont );
	p->setPen( Qt::darkGray );
	QFontMetrics fm( font() );
	QDateTime time( QDate::currentDate(), QTime(QTime::currentTime().hour() + 1, 0) );
	QPointF pos = positionFromTime( time );
	while ( !pos.isNull() ) {
		QString text = KGlobal::locale()->formatTime(time.time());
		int textWidth = fm.width( text );
		QRectF textRect( pos.x() - textWidth / 2.0, pos.y() - fm.height() / 2.0,
						 textWidth, fm.height() );

		Plasma::PaintUtils::drawHalo( p, textRect );
		p->drawText( textRect, text, QTextOption(Qt::AlignCenter) );

		time = time.addSecs( 60 * 60 );
		pos = positionFromTime( time );
	}

	if ( m_showTimetable ) {
		QFont timetableFont = font();
		timetableFont.setBold( false );
		timetableFont.setPixelSize( 10 );
		p->setFont( timetableFont );
#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
		p->setPen( Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor) );
#else
		p->setPen( Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewTextColor) );
#endif

		fm = QFontMetrics( timetableFont );
		qreal padding = 8;
		QRect timetableRect( rect.left() + 5, rect.top() + m_title->boundingRect().height() + 10,
							 rect.width() * 0.4, rect.height() * 0.4 );
		QRect timetableContentsRect = timetableRect.adjusted( padding, padding, -padding, -padding );
		int maxLines = qFloor( timetableRect.height() / fm.lineSpacing() ) - 1;
		QList<DepartureData> departureDataList;
		for ( int i = 0; i < qMin(m_departures.count(), maxLines); ++i ) {
			departureDataList << m_departures[i]->departureData();
		}

		// Draw timetable background
		m_svg.resize( (int)timetableRect.width(), (int)timetableRect.height() );
		m_svg.paint( p, timetableRect, "timetable" );

		// Calculate column widths
		int maxTransportLineWidth = 0;
		int maxDepartureWidth = 0;
		QDateTime currentTime = QDateTime::currentDateTime();
		QStringList departureTimeStrings;
		for ( int i = 0; i < qMin(maxLines, departureDataList.count()); ++i ) {
			int transportLineWidth = fm.width( departureDataList[i].transportLine );
			if ( transportLineWidth > maxTransportLineWidth ) {
				maxTransportLineWidth = transportLineWidth;
			}

			int minsToDeparture = qCeil( currentTime.secsTo(departureDataList[i].time) / 60.0 );
			QString departureTimeString = minsToDeparture == 0 ? i18n("now")
					: i18np("1 min.", "%1 min.", minsToDeparture);
			departureTimeStrings << departureTimeString;

			int departureWidth = fm.width( departureTimeString );
			if ( departureWidth > maxDepartureWidth ) {
				maxDepartureWidth = departureWidth;
			}
		}
		qreal columnTransportLine = qMin( (qreal)maxTransportLineWidth + 5,
										  timetableContentsRect.width() / 4.0 );
		qreal columnDeparture = qMin( (qreal)maxDepartureWidth + 5,
									  timetableContentsRect.width() / 3.5 );
		qreal columnTarget = timetableContentsRect.width() - columnTransportLine - columnDeparture;

		// Prepere text options
		QTextOption textOption( Qt::AlignLeft | Qt::AlignTop );
		textOption.setWrapMode( QTextOption::NoWrap );
		textOption.setTabArray( QList<qreal>() << columnTransportLine
				<< (columnTransportLine + columnTarget) ); // Use an example line string

		// Draw timetable text
		for ( int i = 0; i < qMin(maxLines, departureDataList.count()); ++i ) {
			DepartureData data = departureDataList[i];
			QString elidedLine = fm.elidedText( data.transportLine, Qt::ElideRight, columnTransportLine - 5 );
			QString elidedTarget = fm.elidedText( data.target, Qt::ElideRight, columnTarget - 5 );
			QString departureString = QString("%2\t%3\t%1").arg(departureTimeStrings[i])
										   .arg(elidedLine).arg(elidedTarget);
			QString timetableText( departureString );

			QRect textRect( timetableContentsRect.left(),
					timetableContentsRect.top() + i * fm.lineSpacing(),
					timetableContentsRect.width(), fm.lineSpacing() );
			Plasma::PaintUtils::drawHalo( p, textRect );
			p->drawText( textRect, timetableText, textOption );
		}
	}
}

#include "graphicaltimetableline.moc"
