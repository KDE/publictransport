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

#include "publictransport.h"
#include <QPainter>
#include <QFontMetrics>
#include <QSizeF>
#include <QTextDocument>
#include <KLocale>
#include <QGraphicsLinearLayout>

#include <plasma/svg.h>
#include <plasma/theme.h>
// #include <Plasma/PaintUtils>

#include <Plasma/ToolTipManager>
#include <Plasma/ToolTipContent>
#include <kcolorscheme.h>
#include <QTreeView>

PublicTransport::PublicTransport(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_svg(this),
    m_icon("public-transport-stop"),
    m_graphicsWidget(0),
    m_label(0),
    m_treeView(0),
    m_model(0),
    m_currentSource(""),
    m_useSeperateCityValue(true)
{
    setBackgroundHints(DefaultBackground);
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    m_svg.setImagePath("widgets/background");
    setHasConfigurationInterface(true);  
    resize(300, 200);
}

PublicTransport::~PublicTransport()
{
    if (hasFailedToLaunch()) {
        // Do some cleanup here
    } else {
	delete m_label; // I *think* this gets deleted when I delete m_graphicsWidget...
// 	delete m_treeWidget; // I *think* this gets deleted when I delete m_graphicsWidget...
	delete m_graphicsWidget; // ...which I *think* also gets deleted automatically by plasma?
    }
}

void PublicTransport::init()
{
//     if (m_icon.isNull()) {
//         setFailedToLaunch(true, i18n("No world to say hello"));
//     }

    createTooltip();
    setPopupIcon("public-transport-stop");

    connect(this, SIGNAL(geometryChanged()), this, SLOT(geometryChanged()));
    connect(this, SIGNAL(settingsChanged()), this, SLOT(configChanged()));
   
    // Read config
    KConfigGroup cg = config();
    m_updateTimeout= cg.readEntry("updateTimeout", 60);
    m_showRemainingMinutes= cg.readEntry("showRemainingMinutes", true);
    m_showDepartureTime= cg.readEntry("showDepartureTime", true);
    m_serviceProvider= cg.readEntry("serviceProvider", -1);
    m_city= cg.readEntry("city", "");
    m_stop= cg.readEntry("stop", "");
    m_showTrams = cg.readEntry("showTrams", true);
    m_showBuses = cg.readEntry("showBuses", true);
    m_showNightlines = cg.readEntry("showNightlines", true);
    m_filterMinLine = cg.readEntry("filterMinLine", 1);
    m_filterMaxLine = cg.readEntry("filterMaxLine", 999);
    m_filterTypeDirection = static_cast<FilterType>(cg.readEntry("filterTypeDirection", static_cast<int>(ShowAll)));
    m_filterDirectionList = cg.readEntry("filterDirectionList", QStringList());
    emit settingsChanged();
    
    QAction *actionUpdate = new QAction( KIcon("view-refresh"), i18n("&Update timetable"), this );
    connect( actionUpdate, SIGNAL(triggered(bool)), this, SLOT(actionUpdateTriggered(bool)) );
    addAction( "updateTimetable", actionUpdate );
}

QList< QAction* > PublicTransport::contextualActions()
{
    return QList< QAction* >() << action( "updateTimetable" );
}

void PublicTransport::actionUpdateTriggered ( bool )
{
    reconnectSource();
}

QString PublicTransport::DepartureInfo::getDurationString ( int totalSeconds ) const
{
    int seconds = totalSeconds % 60;
    totalSeconds -= seconds;
    if (seconds > 0)
	totalSeconds += 60;
    if (totalSeconds < 0)
	return i18n("depart is in the past");

    int minutes = (totalSeconds / 60) % 60;
    int hours = totalSeconds / 3600;
    QString str;

    if (hours != 0)
    {
	if (minutes != 0)
	    str = i18n("in %1 hours and %2 minutes", hours, minutes);
// 	    str = i18n("in <b>%1</b> hours and <b>%2</b> minutes", hours, minutes);
	else
	    str = i18np("in %1 hour", "in %1 hours", hours);
// 	    str = i18n("in <b>%1</b> hours", hours);
    }
    else if (minutes != 0)
	str = i18np("in %1 minute", "in %1 minutes", minutes);
// 	str = i18n("in <b>%1</b> minutes", minutes);
    else
	str = i18n("now");

    return str;
}

void PublicTransport::reconnectSource()
{
    if ( !m_currentSource.isEmpty() )
    {
	qDebug() << "PublicTransport::reconnectSource" << "Disconnect data source" << m_currentSource;
	dataEngine("publictransport")->disconnectSource(m_currentSource, this);
    }

    if ( m_useSeperateCityValue )
	m_currentSource = QString("Departures %1:%2:%3").arg( static_cast<int>(m_serviceProvider) ).arg( m_city ).arg( m_stop );
    else
	m_currentSource = QString("Departures %1:%3").arg( static_cast<int>(m_serviceProvider) ).arg( m_stop );
    qDebug() << "PublicTransport::reconnectSource" << "Connect data source" << m_currentSource << "Timeout" << m_updateTimeout;
    dataEngine("publictransport")->connectSource( m_currentSource, this, m_updateTimeout * 1000, Plasma::AlignToMinute );
}

void PublicTransport::dataUpdated ( const QString& sourceName, const Plasma::DataEngine::Data& data )
{
    qDebug() << "PublicTransport::dataUpdated" << sourceName;// << data;
    
    this->setBusy( m_currentTestSource.isEmpty() );
    m_departureInfos.clear();

    if ( !data.value("error").toBool() )
    { // no error
	if ( data.value("receivedPossibleStopList").toBool() )
	{ // list of possible stops
	    if ( !m_currentTestSource.isEmpty() )
	    {
		m_ui.kledStopValidated->setState( KLed::Off );
		m_ui.kledStopValidated->setToolTip( i18n("The stop name is ambigous.") );
		disconnectTestSource();
		
		QStringList possibleStops;
		for (int i = 0; i < data.keys().count(); ++i)
		{
		    QVariant stopData = data.value( QString("stopName %1").arg(i) );
		    if ( !stopData.isValid() )
			break;

		    QMap<QString, QVariant> dataMap = stopData.toMap();
// 		    dataMap["stopName"].toString(), dataMap["stopID"].toInt()
		    possibleStops << dataMap["stopName"].toString();
		}
		m_ui.stop->setCompletedItems( possibleStops );
	    }
	    else
		this->setConfigurationRequired(true, i18n("The stop name is ambigous."));
	}
	else
	{ // list of journeys
	    if ( !m_currentTestSource.isEmpty() && sourceName == m_currentTestSource )
	    {
		m_ui.kledStopValidated->setState( KLed::On );
		m_ui.kledStopValidated->setToolTip( i18n("The stop name is valid.") );
		m_ui.stop->setCompletedItems( QStringList() );
		disconnectTestSource();
		return;
	    }

	    for (int i = 0; i < data.keys().count(); ++i)
	    {
		QVariant departureData = data.value( QString("%1").arg(i) );
		if ( !departureData.isValid() )
		    break;

		QMap<QString, QVariant> dataMap = departureData.toMap();
		DepartureInfo departureInfo( dataMap["line"].toString(), dataMap["direction"].toString(), dataMap["departure"].toTime(), static_cast<LineType>(dataMap["lineType"].toInt()), dataMap["nightline"].toBool(), dataMap["expressline"].toBool() );
		
		m_departureInfos.append( departureInfo );
	    }
	    
	    fillModel();
	}
    }
    else // error
    {
	if ( !m_currentTestSource.isEmpty() )
	{
	    m_ui.kledStopValidated->setState( KLed::Off );
	    m_ui.kledStopValidated->setToolTip( i18n("The stop name is invalid.") );
	    m_ui.stop->setCompletedItems( QStringList() );
	    disconnectTestSource();
	    return;
	}

	this->setConfigurationRequired(true, i18n("Error parsing departure information"));
    }

    createTooltip();
    this->setBusy(false);
}

void PublicTransport::geometryChanged()
{
    if (!isPopupShowing())
	m_label->setMaximumSize(contentsRect().width(), contentsRect().height());
}

void PublicTransport::popupEvent ( bool show )
{
    m_label->setMaximumSize(9999, 9999);
    
    Plasma::PopupApplet::popupEvent ( show );
}

void PublicTransport::createTooltip()
{    
    Plasma::ToolTipContent data;
    data.setMainText( i18n("Public transport") );
    if (m_departureInfos.isEmpty())
	data.setSubText( i18n("View departure times for public transport") );
    else
    {
	DepartureInfo nextDeparture = m_departureInfos.first();
	data.setSubText( i18nc("%4 is the translated duration text, e.g. in 3 minutes", "Next departure from '%1': line %2 (%3) %4", m_stop, nextDeparture.lineString, nextDeparture.direction , nextDeparture.duration ) );
    }
    data.setImage( KIcon("public-transport-stop").pixmap(IconSize(KIconLoader::Desktop)) );
    Plasma::ToolTipManager::self()->setContent(this, data);
}

void PublicTransport::configChanged()
{
    m_departureInfos.clear();

    Plasma::DataEngine::Data data = dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString sKey, data.keys() ) // The keys are countries
    {
	QMap< QString, QVariant > mapServiceProviders = data.value(sKey).toMap();
	foreach ( QString sServiceProvider, mapServiceProviders.keys() )
	{
	    QMap< QString, QVariant > serviceProviderData = mapServiceProviders[sServiceProvider].toMap();
	    if ( m_serviceProvider == serviceProviderData["id"].toInt() )
	    {
		m_useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
		goto doublebreak;
	    }
	}
    }
doublebreak:
	    
    if ( checkConfig() )
	reconnectSource();
}

bool PublicTransport::checkConfig()
{
    if ( m_useSeperateCityValue && (m_city.isEmpty() || m_stop.isEmpty()) )
	this->setConfigurationRequired(true, i18n("Please set a city and a stop."));
    else if ( m_stop.isEmpty() )
	this->setConfigurationRequired(true, i18n("Please set a stop."));
    else if ( m_serviceProvider == -1 )
	this->setConfigurationRequired(true, i18n("Please select a service provider."));
    else if ( m_filterMinLine > m_filterMaxLine )
	this->setConfigurationRequired(true, i18n("The minimal shown line can't be bigger than the maximal shown line."));
    else
    {
	this->setConfigurationRequired(false);
	return true;
    }

    return false;
}

QGraphicsWidget* PublicTransport::graphicsWidget()
{
    if (!m_graphicsWidget) {
	m_graphicsWidget = new QGraphicsWidget(this);
        m_graphicsWidget->setMinimumSize(200, 125);
        m_graphicsWidget->setPreferredSize(350, 200);
	m_label = new Plasma::Label();
	m_label->setAlignment(Qt::AlignTop);

	// Create treeview
	m_treeView = new Plasma::TreeView();
	m_treeView->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	QTreeView *treeView = (QTreeView*)m_treeView->widget();
	treeView->setAlternatingRowColors( true );
	treeView->setAllColumnsShowFocus( false );
	treeView->setRootIsDecorated( false );
	treeView->setSortingEnabled( true );
	treeView->setSelectionMode( QAbstractItemView::NoSelection );
	treeView->setSelectionBehavior( QAbstractItemView::SelectRows );
	treeView->header()->setCascadingSectionResizes( true );
	treeView->header()->setResizeMode( QHeaderView::ResizeToContents );

	// Get colors
	QColor textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
	QColor baseColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::BackgroundColor );
	QColor altBaseColor = baseColor.darker();
	int green = altBaseColor.green() * 1.8;
	altBaseColor.setGreen( green > 255 ? 255 : green ); // tint green
	QColor buttonColor = altBaseColor;
	baseColor.setAlpha(50);
	altBaseColor.setAlpha(50);
	buttonColor.setAlpha(150);

	// Set colors
	QPalette p = palette();
	p.setColor( QPalette::Base, baseColor );
	p.setColor( QPalette::AlternateBase, altBaseColor );
	p.setColor( QPalette::Button, buttonColor );
	p.setColor( QPalette::Foreground, textColor );
	p.setColor( QPalette::Text, textColor );
	treeView->setPalette(p);

	// Create model
	m_model = new QStandardItemModel( 0, 3 );
	m_model->setHorizontalHeaderLabels( QStringList()
	    << i18nc("A tramline or busline", "Line")
	    << i18nc("Direction of a tramline or busline", "Direction")
	    << i18nc("Time of departure of a tram or bus", "Departure") );
	m_model->horizontalHeaderItem(0)->setTextAlignment( Qt::AlignRight );
	m_model->horizontalHeaderItem(0)->setSizeHint( QSize(10,10) );
	m_model->horizontalHeaderItem(0)->setForeground( QBrush(textColor) );
	m_model->horizontalHeaderItem(1)->setForeground( QBrush(textColor) );
	m_model->horizontalHeaderItem(2)->setForeground( QBrush(textColor) );
	m_model->setSortRole( Qt::UserRole );
	m_treeView->setModel( m_model );
	m_model->sort( 2, Qt::AscendingOrder );
	treeView->header()->resizeSection( 0, 50 );
	treeView->header()->setPalette(p);

	QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
	layout->addItem(m_label);
	layout->setAlignment(m_label, Qt::AlignTop);
	layout->addItem(m_treeView);
	m_graphicsWidget->setLayout(layout);
	
	registerAsDragHandle(m_graphicsWidget);
	registerAsDragHandle(m_label);
// 	registerAsDragHandle(m_treeView);
    }

    return m_graphicsWidget;
}

void PublicTransport::constraintsEvent(Plasma::Constraints constraints)
{
    if (!m_graphicsWidget) {
        graphicsWidget();
    }

    if ((constraints|Plasma::FormFactorConstraint || constraints|Plasma::SizeConstraint) &&
        layout()->itemAt(0) != m_graphicsWidget) {
    }
}

void PublicTransport::paintInterface(QPainter *p,
        const QStyleOptionGraphicsItem *option, const QRect &contentsRect)
{
//     p->setRenderHint(QPainter::SmoothPixmapTransform);
//     p->setRenderHint(QPainter::Antialiasing);

    // Now we draw the applet, starting with our svg
//     m_svg.resize((int)contentsRect.width(), (int)contentsRect.height());
//     m_svg.paint(p, (int)contentsRect.left(), (int)contentsRect.top());

    // We place the icon and text
//     p->drawPixmap(7, 0, m_icon.pixmap((int)contentsRect.width(),(int)contentsRect.width()-14));
//     p->save();
//     p->setPen(Qt::black);

//     QTextDocument document;
//     document.setHtml(m_infoText);
//     document.setPageSize(contentsRect.size());
//     p->translate(contentsRect.topLeft());
//     document.drawContents(p);

//     QPainter *painter = new QPainter(m_widget);
//     document.drawContents(painter);
//     delete painter;
//     p->drawText(contentsRect,
//                 Qt::AlignBottom | Qt::AlignHCenter,
// 		m_infoText);
//                 i18n("Hello Plasmoid!"));
//     p->restore();
}

void PublicTransport::createConfigurationInterface ( KConfigDialog* parent )
{
    QWidget *widget = new QWidget;
    QWidget *widgetFilter = new QWidget;

    m_ui.setupUi(widget);
    m_uiFilter.setupUi(widgetFilter);
    
    connect(parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(configAccepted()));
    parent->addPage(widget, i18n("General"), "public-transport-stop");
    parent->addPage(widgetFilter, i18n("Filter"), "view-filter");

    if (m_updateTimeout == 0) {
	m_ui.updateAutomatically->setChecked(false);
	m_ui.updateTimeout->setValue(60); // Set to default
    }
    else {
	m_ui.updateAutomatically->setChecked(true);
	m_ui.updateTimeout->setValue(m_updateTimeout);
    }
    m_ui.showRemainingMinutes->setChecked(m_showRemainingMinutes);
    m_ui.showDepartureTime->setChecked(m_showDepartureTime);
    m_ui.city->setText(m_city);
    m_ui.stop->setText(m_stop);

    Plasma::DataEngine::Data data = dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString sKey, data.keys() ) // The keys are countries
    {
	QMap< QString, QVariant > mapServiceProviders = data.value(sKey).toMap();
	foreach ( QString sServiceProvider, mapServiceProviders.keys() )
	{
	    QMap< QString, QVariant > serviceProviderData = mapServiceProviders[sServiceProvider].toMap();
	    m_ui.serviceProvider->addItem( sServiceProvider, serviceProviderData );
	}
    }

    // Get (combobox-) index of the currently selected service provider
    int curServiceProviderIndex = -1;
    for ( int i = 0; i < m_ui.serviceProvider->count(); ++i )
    {
	if ( m_ui.serviceProvider->itemData( i ).toMap()["id"].toInt() == m_serviceProvider )
	{
	    curServiceProviderIndex = i;
	    break;
	}
    }
    connect( m_ui.serviceProvider, SIGNAL(currentIndexChanged(int)), this, SLOT(serviceProviderChanged(int)) );
    if ( curServiceProviderIndex != -1 )
	m_ui.serviceProvider->setCurrentIndex( curServiceProviderIndex );

    m_uiFilter.showTrams->setChecked(m_showTrams);
    m_uiFilter.showBuses->setChecked(m_showBuses);
    m_uiFilter.showNightLines->setChecked(m_showNightlines);
    m_uiFilter.filterMinLine->setValue(m_filterMinLine);
    m_uiFilter.filterMaxLine->setValue(m_filterMaxLine);
    m_uiFilter.filterTypeTarget->setCurrentIndex(m_filterTypeDirection);
    m_uiFilter.filterTargetList->setItems(m_filterDirectionList);

    connect( m_ui.stop, SIGNAL(userTextChanged(QString)), this, SLOT(stopNameChanged(QString)) );
}

void PublicTransport::disconnectTestSource()
{
    if ( !m_currentTestSource.isEmpty() && m_currentTestSource != m_currentSource )
    {
	qDebug() << "PublicTransport::disconnectTestSource" << "disconnecting source" << m_currentTestSource;
	dataEngine("publictransport")->disconnectSource( m_currentTestSource, this );
    }
    m_currentTestSource = "";
}

void PublicTransport::serviceProviderChanged ( int index )
{
    m_ui.kledStopValidated->setState( KLed::Off );
    m_ui.kledStopValidated->setToolTip( i18n("Checking validity of the stop name.") );
    
    bool useSeperateCityValue = m_ui.serviceProvider->itemData( index ).toMap()["useSeperateCityValue"].toBool();
    m_ui.lblCity->setVisible( useSeperateCityValue );
    m_ui.city->setVisible( useSeperateCityValue );

    if ( useSeperateCityValue )
    {
	QStringList cities = m_ui.serviceProvider->itemData( index ).toMap()["cities"].toStringList();
	if ( !cities.isEmpty() )
	{
	    m_ui.city->setText( cities.first() );
	    m_ui.city->setCompletedItems( cities, false );
	}
    }
    else
	m_ui.city->setText( "" );
}

void PublicTransport::stopNameChanged ( QString stopName )
{
    disconnectTestSource();

    bool useSeperateCityValue = m_ui.serviceProvider->itemData( m_ui.serviceProvider->currentIndex() ).toMap()["useSeperateCityValue"].toBool();
    int serviceProviderIndex = m_ui.serviceProvider->itemData( m_ui.serviceProvider->currentIndex() ).toMap()["id"].toInt();
    if ( useSeperateCityValue )
    {
	QStringList values = stopName.split(',');
	if ( values.count() > 1 )
	{
	    if ( m_serviceProvider == 10 ) // switzerland
	    {
		m_ui.city->setText( values[1].trimmed() );
		m_ui.stop->setText( values[0].trimmed() );
	    }
	    else
	    {
		m_ui.city->setText( values[0].trimmed() );
		m_ui.stop->setText( values[1].trimmed() );
	    }
	}
	m_currentTestSource = QString("Departures %1:%2:%3").arg( serviceProviderIndex ).arg( m_ui.city->text() ).arg( stopName );
    }
    else
	m_currentTestSource = QString("Departures %1:%3").arg( serviceProviderIndex ).arg( stopName );

    // Check if the stop name is ambigous. If it is get a list of possible stops.
    qDebug() << "PublicTransport::stopNameChanged" << "connecting source" << m_currentTestSource;
    dataEngine("publictransport")->connectSource( m_currentTestSource, this );
}

void PublicTransport::configAccepted()
{
    bool changed = false;
    
    if (!m_ui.updateAutomatically->isChecked() && m_updateTimeout != 0) {
        m_updateTimeout = 0;
        KConfigGroup cg = config();
        cg.writeEntry("updateTimeout", m_updateTimeout);
        changed = true;
    }
    else if (m_updateTimeout  != m_ui.updateTimeout->value()) {
        m_updateTimeout = m_ui.updateTimeout->value();
        KConfigGroup cg = config();
        cg.writeEntry("updateTimeout", m_updateTimeout);
        changed = true;
    }
    
    if (m_showRemainingMinutes != (m_ui.showRemainingMinutes->checkState() == Qt::Checked)) {
        m_showRemainingMinutes = !m_showRemainingMinutes;;
        KConfigGroup cg = config();
        cg.writeEntry("showRemainingMinutes", m_showRemainingMinutes);
        changed = true;
    }
    
    if (m_showDepartureTime != (m_ui.showDepartureTime->checkState() == Qt::Checked)) {
        m_showDepartureTime = !m_showDepartureTime;;
        KConfigGroup cg = config();
        cg.writeEntry("showDepartureTime", m_showDepartureTime);
        changed = true;
    }

    if (m_city  != m_ui.city->text()) {
        m_city = m_ui.city->text();
        KConfigGroup cg = config();
        cg.writeEntry("city", m_city);
        changed = true;
    }

    if (m_stop  != m_ui.stop->text()) {
        m_stop = m_ui.stop->text();
        KConfigGroup cg = config();
        cg.writeEntry("stop", m_stop);
        changed = true;
    }

    const int serviceProviderIndex = m_ui.serviceProvider->itemData( m_ui.serviceProvider->currentIndex() ).toMap()["id"].toInt();
    if (m_serviceProvider != serviceProviderIndex) {
        m_serviceProvider = serviceProviderIndex;
        KConfigGroup cg = config();
        cg.writeEntry("serviceProvider", m_serviceProvider);
        changed = true;

	m_useSeperateCityValue = m_ui.serviceProvider->itemData( m_ui.serviceProvider->currentIndex() ).toMap()["useSeperateCityValue"].toBool();
    }

    if (m_showTrams != (m_uiFilter.showTrams->checkState() == Qt::Checked)) {
        m_showTrams = !m_showTrams;;
        KConfigGroup cg = config();
        cg.writeEntry("showTrams", m_showTrams);
        changed = true;
    }

    if (m_showBuses != (m_uiFilter.showBuses->checkState() == Qt::Checked)) {
        m_showBuses = !m_showBuses;
        KConfigGroup cg = config();
        cg.writeEntry("showBuses", m_showBuses);
        changed = true;
    }

    if (m_showNightlines != (m_uiFilter.showNightLines->checkState() == Qt::Checked)) {
        m_showNightlines = !m_showNightlines;
        KConfigGroup cg = config();
        cg.writeEntry("showNightlines", m_showNightlines);
        changed = true;
    }

    if (m_filterMinLine  != m_uiFilter.filterMinLine->value()) {
        m_filterMinLine = m_uiFilter.filterMinLine->value();
        KConfigGroup cg = config();
        cg.writeEntry("filterMinLine", m_filterMinLine);
        changed = true;
    }

    if (m_filterMaxLine  != m_uiFilter.filterMaxLine->value()) {
        m_filterMaxLine = m_uiFilter.filterMaxLine->value();
        KConfigGroup cg = config();
        cg.writeEntry("filterMaxLine", m_filterMaxLine);
        changed = true;
    }

    if (m_filterTypeDirection  != (m_uiFilter.filterTypeTarget->currentIndex())) {
        m_filterTypeDirection = static_cast<FilterType>(m_uiFilter.filterTypeTarget->currentIndex());
        KConfigGroup cg = config();
        cg.writeEntry("filterTypeDirection", static_cast<int>(m_filterTypeDirection));
        changed = true;
    }
    
    if (m_filterDirectionList  != m_uiFilter.filterTargetList->items()) {
        m_filterDirectionList = m_uiFilter.filterTargetList->items();
	m_filterDirectionList.removeDuplicates();
        KConfigGroup cg = config();
        cg.writeEntry("filterDirectionList", m_filterDirectionList);
        changed = true;
    }
    
    if (changed) {
        emit settingsChanged();
        emit configNeedsSaving();
//         update();
    }
}

void PublicTransport::fillModel()
{
    if (m_model == NULL)
	graphicsWidget();
    
    m_model->removeRows(0, m_model->rowCount());

    //     dataEngine("time")
    QString sServiceProvider;
    Plasma::DataEngine::Data data = dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString sKey, data.keys() ) // The keys are countries
    {
	QMap< QString, QVariant > mapServiceProviders = data.value(sKey).toMap();
	foreach ( QString sCurServiceProvider, mapServiceProviders.keys() )
	{
	    if ( mapServiceProviders[sCurServiceProvider].toMap()["id"].toInt() == m_serviceProvider )
	    {
		sServiceProvider = sCurServiceProvider;
		QRegExp rx( "(?:\\()([^\\)]+)(?:\\))" );
		rx.indexIn(sServiceProvider);
		sServiceProvider = rx.cap(1);
		goto found;
	    }
	}
    }
found:
//     QString cityInfo = "<font size='-1'>(%2, %3)</font>" //.arg(m_city)
    m_infoText = QString("<html><body><center><big><b>%1</b></big></center> <center><small>(last update: %2, data by %3)</small></center></body></html>").arg( m_stop ).arg( QTime::currentTime().toString("hh:mm") ).arg( sServiceProvider );
    m_label->setText( m_infoText );
    
    bool oddRow = true;
    foreach( DepartureInfo departureInfo, m_departureInfos )
    {
	// Apply filters
	if( (departureInfo.lineType == Tram && !m_showTrams) ||
	    (departureInfo.lineType == Bus && !m_showBuses) ||
	    (departureInfo.isNightLine() && !m_showNightlines) ||
	    (departureInfo.lineNumber != 0 && (departureInfo.lineNumber < m_filterMinLine || departureInfo.lineNumber > m_filterMaxLine)) ||
	    (m_filterTypeDirection == ShowMatching && !m_filterDirectionList.contains(departureInfo.direction)) ||
	    (m_filterTypeDirection == HideMatching && m_filterDirectionList.contains(departureInfo.direction)) )
	    continue;
	QString sTime, time, sDeparture = departureInfo.departure.toString("hh:mm");
	if (m_showDepartureTime && m_showRemainingMinutes)
	    sTime = QString("%1 (%2)").arg( sDeparture ).arg( departureInfo.duration );
	else if (m_showDepartureTime)
	    sTime = sDeparture;
	else if (m_showRemainingMinutes)
	    sTime = departureInfo.duration;
	else
	    time = sTime = "";
	
	QStandardItem *itemLineString, *itemDirection, *itemDeparture;
	itemLineString = new QStandardItem( departureInfo.lineString );
	QFont font = itemLineString->font();
	font.setBold(true);
	itemLineString->setFont(font);
	itemLineString->setTextAlignment( Qt::AlignRight );
	itemLineString->setData(itemLineString->text(), Qt::UserRole);
	if ( departureInfo.lineNumber != Unknown )
	    itemLineString->setIcon( iconFromLineType(departureInfo.lineType) );
	itemDirection = new QStandardItem( departureInfo.direction );
	itemDirection->setData(itemDirection->text(), Qt::UserRole);
	itemDeparture = new QStandardItem( sTime );
	itemDeparture->setData(departureInfo.departure, Qt::UserRole);
	m_model->appendRow( QList< QStandardItem* >()
	    << itemLineString << itemDirection << itemDeparture );

	oddRow = !oddRow;
    }
}

#include "publictransport.moc"
