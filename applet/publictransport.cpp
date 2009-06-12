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

#include <Plasma/ToolTipManager>
#include <Plasma/ToolTipContent>


PublicTransport::PublicTransport(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_svg(this),
    m_icon("public-transport-stop"),
    m_graphicsWidget(0),
    m_label(0),
    m_currentSource("")
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
	delete m_graphicsWidget; // ...which I *think* also gets deleted automatically by plasma?
    }
}

void PublicTransport::init()
{
    if (m_icon.isNull()) {
        setFailedToLaunch(true, i18n("No world to say hello"));
    }

    createTooltip();
    setPopupIcon("public-transport-stop");

    connect(this, SIGNAL(geometryChanged()), this, SLOT(geometryChanged()));
    connect(this, SIGNAL(settingsChanged()), this, SLOT(reload()));
   
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

    reconnectSource();
}

void PublicTransport::reconnectSource()
{
    if ( !m_currentSource.isEmpty() )
	dataEngine("publictransport")->disconnectSource(m_currentSource, this);
    
    m_currentSource = QString("Departures %1:%2:%3").arg( static_cast<int>(m_serviceProvider) ).arg( m_city ).arg( m_stop );
    dataEngine("publictransport")->connectSource( m_currentSource, this, m_updateTimeout * 1000, Plasma::AlignToMinute );
//     qDebug() << "Data source connected" << m_currentSource << "Timeout" << m_updateTimeout;
}

void PublicTransport::dataUpdated ( const QString& sourceName, const Plasma::DataEngine::Data& data )
{
    this->setBusy(true);

    m_departureInfos.clear();
    for (int i = 0; i < data.keys().count(); ++i)
    {
	QMap<QString, QVariant> dataMap = data.value( QString("%1").arg(i) ).toMap();
	DepartureInfo departureInfo( dataMap["line"].toString(), dataMap["direction"].toString(), dataMap["departure"].toTime() );

	m_departureInfos.append( departureInfo );
    }

    generateInfoText();
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
    m_label->setMaximumSize(999, 999);
    
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

/* Reloads the departure information from the network and restarts the auto reload timer.
*/
void PublicTransport::reload()
{
    m_departureInfos.clear();
    
    if (checkConfig())
	reconnectSource();
}

/* Returns true, if all settings are ok, otherwise it returns false.
*/
bool PublicTransport::checkConfig()
{
    if ( m_city.isEmpty() || m_stop.isEmpty() )
	this->setConfigurationRequired(true, i18n("Please set a city and a stop."));
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
        m_graphicsWidget->setPreferredSize(300, 200);
	m_label = new Plasma::Label();
	m_label->setAlignment(Qt::AlignTop);
	
	QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
	layout->addItem(m_label);
	layout->setAlignment(m_label, Qt::AlignTop);
	m_graphicsWidget->setLayout(layout);
	
	registerAsDragHandle(m_graphicsWidget);
	registerAsDragHandle(m_label);
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
    p->setRenderHint(QPainter::SmoothPixmapTransform);
    p->setRenderHint(QPainter::Antialiasing);

    // Now we draw the applet, starting with our svg
//     m_svg.resize((int)contentsRect.width(), (int)contentsRect.height());
//     m_svg.paint(p, (int)contentsRect.left(), (int)contentsRect.top());

    // We place the icon and text
//     p->drawPixmap(7, 0, m_icon.pixmap((int)contentsRect.width(),(int)contentsRect.width()-14));
    p->save();
    p->setPen(Qt::black);

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
    p->restore();
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
	    m_ui.serviceProvider->addItem( sServiceProvider, mapServiceProviders[sServiceProvider].toInt() );
    }

    // Get (combobox-) index of the currently selected service provider
    const int curServiceProviderIndex = m_ui.serviceProvider->findData( m_serviceProvider );
    m_ui.serviceProvider->setCurrentIndex( curServiceProviderIndex );

    m_uiFilter.showTrams->setChecked(m_showTrams);
    m_uiFilter.showBuses->setChecked(m_showBuses);
    m_uiFilter.showNightLines->setChecked(m_showNightlines);
    m_uiFilter.filterMinLine->setValue(m_filterMinLine);
    m_uiFilter.filterMaxLine->setValue(m_filterMaxLine);
    m_uiFilter.filterTypeTarget->setCurrentIndex(m_filterTypeDirection);
    m_uiFilter.filterTargetList->setItems(m_filterDirectionList);
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

    const int serviceProviderIndex = m_ui.serviceProvider->itemData( m_ui.serviceProvider->currentIndex() ).toInt();
    if (m_serviceProvider != serviceProviderIndex) {
        m_serviceProvider = serviceProviderIndex;
        KConfigGroup cg = config();
        cg.writeEntry("serviceProvider", m_serviceProvider);
        changed = true;
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

void PublicTransport::generateInfoText()
{
    m_infoText = QString("<html><body><center><big><b>%1</b> <font size='-1'>(%2)</font></big></center><table style='margin-top: 8px;' cellpadding='3' cellspacing='0' width='100%'>").arg(m_stop).arg(m_city);
    m_infoText += "<tr style='background-color:rgba(100,225,100, 100); border:1px solid black;'><th align='right'><b>" + i18nc("A tramline or busline", "Line") + "</b></th> <th align='left'><b>" + i18nc("Direction of a tramline or busline", "Direction") + "</b></th>";
    if (m_showDepartureTime || m_showRemainingMinutes)
	m_infoText += "<th align='left'><b>" + i18nc("Time of departure of a tram or bus", "Departure") + "</b></th>";
    m_infoText += "</tr>";
    
    if (m_departureInfos.isEmpty())
	m_infoText += i18n("Error parsing departure information");
    else
    {
	bool oddRow = true;
	foreach( DepartureInfo departureInfo, m_departureInfos )
	{
	    // TODO: reimplement..
// 	    if( (departureInfo.lineType() == DepartureInfo::Tram && !m_showTrams) ||
// 		(departureInfo.lineType() == DepartureInfo::Bus && !m_showBuses) ||
// 		(departureInfo.isNightLine() && !m_showNightlines) ||
// 		departureInfo.line() < m_filterMinLine || departureInfo.line() > m_filterMaxLine ||
// 		(m_filterTypeDirection == ShowMatching && !m_filterDirectionList.contains(departureInfo.direction())) ||
// 		(m_filterTypeDirection == HideMatching && m_filterDirectionList.contains(departureInfo.direction())) )
// 		continue;

	    QString time, sDeparture = departureInfo.departure.toString("hh:mm");
	    if (m_showDepartureTime && m_showRemainingMinutes)
		time = QString("<td>%1 (%2)</td>").arg( sDeparture ).arg( departureInfo.duration );
	    else if (m_showDepartureTime)
		time = QString("<td>%1</td>").arg( sDeparture );
	    else if (m_showRemainingMinutes)
		time = QString("<td>%1</td>").arg( departureInfo.duration );
	    else
		time = "";

	    const QString sRowStyle = oddRow ? " style='background-color:rgba(255,255,255, 50);'"
							    : " style='background-color:rgba(100,225,100, 50);'";
	    m_infoText += QString("<tr%1><td align='right' style='color:#444400;'><b>%2</b></td><td style='color:#005500;'>%3</td>%4</tr>").arg(sRowStyle).arg( departureInfo.lineString ).arg( departureInfo.direction ).arg( time );

	    oddRow = !oddRow;
	}
    }
    m_infoText += "</table></body></html>";

    if (m_label == NULL)
	graphicsWidget();
    m_label->setText(m_infoText);
}

#include "publictransport.moc"
