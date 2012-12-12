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

// Header
#include "gtfsdatabasetab.h"

// Own includes
#include "../project.h"

// Public Transport engine includes
#include <engine/serviceproviderglobal.h>
#include <engine/gtfs/gtfsdatabase.h>

// KDE includes
#include <KIcon>
#include <KConfig>
#include <KConfigGroup>
#include <KDebug>
#include <KComboBox>
#include <KLocalizedString>
#include <KTabWidget>
#include <KGlobal>
#include <KLocale>
#include <KPushButton>
#include <KMessageBox>
#include <Plasma/DataEngineManager>
#include <Plasma/ServiceJob>
#include <KStandardDirs>
#include <KTextEdit>
#include <kdeclarative.h>

// Qt includes
#include <QTableView>
#include <QBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>
#include <QToolButton>
#include <QSqlTableModel>
#include <QSqlQuery>
#include <QFileInfo>
#include <qdeclarative.h>
#include <QDeclarativeView>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>

GtfsDatabaseTab::GtfsDatabaseTab( Project *project, QWidget *parent )
        : AbstractTab(project, type(), parent), m_model(0), m_queryModel(0), m_tabWidget(0),
          m_qmlView(0), m_tableChooser(0), m_tableView(0), m_queryTableView(0), m_query(0)
{
    // Create a tab widget with tabs at the left, because it gets shown in a tab in timetablemate
    m_tabWidget = new KTabWidget( parent );
    m_tabWidget->setTabPosition( QTabWidget::West );
    m_tabWidget->setIconSize( QSize(24, 24) );
    setWidget( m_tabWidget );

    // Find the QML file used for the dashboard tab
    const QString fileName = KGlobal::dirs()->findResource( "data", "timetablemate/gtfs_dashboard.qml" );
    if ( fileName.isEmpty() ) {
        kWarning() << "gtfs_dashboard.qml not found! Check installation";
        return;
    }
    const QString svgFileName = KGlobal::dirs()->findResource( "data", "timetablemate/dashboard.svg" );

    // Register classes in Qt's meta object system and for QML
    qRegisterMetaType< const ServiceProviderData* >( "const ServiceProviderData*" );
    qRegisterMetaType< Project* >( "Project*" );
    qmlRegisterType< ServiceProviderData, 1 >( "TimetableMate", 1, 0, "ServiceProviderData" );
    qmlRegisterUncreatableType< Project >( "TimetableMate", 1, 0, "Project",
                                           "Cannot create new projects" );
    qmlRegisterUncreatableType< Tabs >( "TimetableMate", 1, 0, "Tabs", "Only for enumerables" );
    qmlRegisterUncreatableType< Enums >( "TimetableMate", 1, 0, "PublicTransport",
                                         "Only for enumerables" );

    // Create dashboard widget
    QWidget *container = new QWidget( parent );
    m_qmlView = new QDeclarativeView( container );

    // Install a KDeclarative instance to allow eg. QIcon("icon"), i18n("translate")
    KDeclarative *kdeclarative = new KDeclarative();
    kdeclarative->setDeclarativeEngine( m_qmlView->engine() );
    kdeclarative->initialize();
    kdeclarative->setupBindings();

    m_qmlView->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    m_qmlView->setResizeMode( QDeclarativeView::SizeRootObjectToView );

    // Expose the project itself to QML,
    // because the declarative engine runs in another thread, Project needs to be thread safe
    m_qmlView->rootContext()->setContextProperty( "project", project );

    // Expose the name of the SVG to use
    m_qmlView->rootContext()->setContextProperty( "svgFileName", svgFileName );

    // Add Plasma QML import paths
    const QStringList importPaths = KGlobal::dirs()->findDirs( "module", "imports" );
    foreach( const QString &importPath, importPaths ) {
        m_qmlView->engine()->addImportPath( importPath );
    }

    m_qmlView->setSource( fileName );

    // Create tab showing database tables
    QWidget *tableTab = new QWidget( m_tabWidget );
    m_tableView = new QTableView( tableTab );
    m_tableView->setEditTriggers( QAbstractItemView::NoEditTriggers );
    m_tableView->setSortingEnabled( true );

    m_tableChooser = new KComboBox( tableTab );
    m_tableChooser->addItem( KIcon("table"), i18nc("@info/plain", "Agency(s)"), "agency" );
    m_tableChooser->addItem( KIcon("table"), i18nc("@info/plain", "Stops"), "stops" );
    m_tableChooser->addItem( KIcon("table"),
            i18nc("@info/plain", "Routes (groups of trips)"), "routes" );
    m_tableChooser->addItem( KIcon("table"),
            i18nc("@info/plain", "Trips (sequences of two or more stops"), "trips" );
    m_tableChooser->addItem( KIcon("table"), i18nc("@info/plain", "Stop Times"), "stop_times" );
    m_tableChooser->addItem( KIcon("table"),
            i18nc("@info/plain", "Calendar (service dates with weekly schedule)"), "calendar" );
    m_tableChooser->addItem( KIcon("table"),
            i18nc("@info/plain", "Calendar Dates (exceptions for weekly schedules services)"),
            "calendar_dates" );
    m_tableChooser->addItem( KIcon("table"),
            i18nc("@info/plain", "Fare Attributes"), "fare_attributes" );
    m_tableChooser->addItem( KIcon("table"), i18nc("@info/plain", "Fare Rules"), "fare_rules" );
//     m_tableChooser->addItem( KIcon("table"), i18nc("@info/plain", "Shapes"), "shapes" ); // Not used
    m_tableChooser->addItem( KIcon("table"), i18nc("@info/plain", "Frequencies"), "frequencies" );
    m_tableChooser->addItem( KIcon("table"), i18nc("@info/plain", "Transfers"), "transfers" );
//     m_tableChooser->addItem( KIcon("table"), i18nc("@info/plain", "Feed Info"), "feed_info" ); // Not used

    // These strings are taken from https://developers.google.com/transit/gtfs/reference?hl=de
    // (Feed Files), feed file names are replaced by the names used by the table chooser combobox
    m_tableChooser->setToolTip( i18nc("@info:tooltip",
            "<title>Choose a table of the GTFS database</title>"
            "<para><list>"
            "<item><interface>Agency(s)</interface>: "
                "One or more transit agencies that provide the data in this feed.</item>"
            "<item><interface>Stops</interface>: "
                "Individual locations where vehicles pick up or drop off passengers.</item>"
            "<item><interface>Routes</interface>: "
                "Transit routes. A route is a group of trips that are displayed to riders "
                "as a single service.</item>"
            "<item><interface>Trips</interface>: "
                "Trips for each route. A trip is a sequence of two or more stops that occurs "
                "at specific time.</item>"
            "<item><interface>Stop Times</interface>: "
                "Times that a vehicle arrives at and departs from individual stops "
                "for each trip.</item>"
            "<item><interface>Calendar</interface>: "
                "Dates for service IDs using a weekly schedule. Specify when service starts "
                "and ends, as well as days of the week where service is available.</item>"
            "<item><interface>Calendar Dates</interface>: "
                "Exceptions for the service IDs defined in the calendar.txt file. "
                "If <interface>Calendar Dates</interface> includes <emphasize>all</emphasize> "
                "dates of service, this file may be specified instead of "
                "<interface>Calendar</interface>.</item>"
            "<item><interface>Fare Attributes</interface>: "
                "Fare information for a transit organization's routes.</item>"
            "<item><interface>Fare Rules</interface>: "
                "Rules for applying fare information for a transit organization's routes.</item>"
            "<item><interface>Shapes</interface> (not used): "
                "Rules for drawing lines on a map to represent "
                "a transit organization's routes.</item>"
            "<item><interface>Frequencies</interface>: "
                "Headway (time between trips) for routes with variable frequency of service.</item>"
            "<item><interface>Transfers</interface>: "
                "Rules for making connections at transfer points between routes.</item>"
            "<item><interface>Feed Info</interface> (not used): "
                "Additional information about the feed itself, including publisher, version, "
                "and expiration information.</item>"
            "</list></para>") );
    connect( m_tableChooser, SIGNAL(currentIndexChanged(int)), this, SLOT(tableChosen(int)) );

    QVBoxLayout *vboxLayout = new QVBoxLayout( tableTab );
    vboxLayout->addWidget( m_tableChooser );
    vboxLayout->addWidget( m_tableView );

    // Create tab to execute database queries
    QWidget *queryTab = new QWidget( m_tabWidget );
    m_query = new KTextEdit( queryTab );
    m_query->setClickMessage( i18nc("@info/plain", "Enter an SQLite database query...") );
    m_query->setFixedHeight( m_query->fontMetrics().height() * 4 );
    QToolButton *runQueryButton = new QToolButton( queryTab );
    runQueryButton->setIcon( KIcon("system-run") );
    runQueryButton->setText( i18nc("@info/plain", "&Execute Query") );
    runQueryButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    connect( runQueryButton, SIGNAL(clicked(bool)), this, SLOT(executeQuery()) );

    m_queryTableView = new QTableView( queryTab );
    m_queryTableView->setEditTriggers( QAbstractItemView::NoEditTriggers );
    m_queryTableView->setSortingEnabled( true );

    QHBoxLayout *hboxLayoutQuery = new QHBoxLayout();
    hboxLayoutQuery->addWidget( m_query );
    hboxLayoutQuery->addWidget( runQueryButton );
    QVBoxLayout *vboxLayoutQuery = new QVBoxLayout( queryTab );
    vboxLayoutQuery->addLayout( hboxLayoutQuery );
    vboxLayoutQuery->addWidget( m_queryTableView );

    m_tabWidget->addTab( m_qmlView, KIcon("dashboard-show"), i18nc("@title:tab", "Overview") );
    m_tabWidget->addTab( tableTab, KIcon("server-database"), i18nc("@title:tab", "Database") );
    m_tabWidget->addTab( queryTab, KIcon("system-run"), i18nc("@title:tab", "Query") );

    // Disable database tab, until m_state changes to ImportFinished
    m_tabWidget->setTabEnabled( 1, false );
    gtfsDatabaseStateChanged( project->gtfsDatabaseState() );
    connect( project, SIGNAL(gtfsDatabaseStateChanged(Project::GtfsDatabaseState)),
             this, SLOT(gtfsDatabaseStateChanged(Project::GtfsDatabaseState)) );
}

GtfsDatabaseTab::~GtfsDatabaseTab()
{
}

GtfsDatabaseTab *GtfsDatabaseTab::create( Project *project, QWidget *parent )
{
    // Create GTFS database tab
    return new GtfsDatabaseTab( project, parent );
}

void GtfsDatabaseTab::executeQuery()
{
    if ( !m_queryModel ) {
        kWarning() << "No database connection";
        return;
    }

    const QString query = m_query->toPlainText();
    m_queryModel->setQuery( query, GtfsDatabase::database(project()->data()->id()) );
}

void GtfsDatabaseTab::tableChosen( int index )
{
    if ( !m_model ) {
        kWarning() << "No database connection";
        return;
    }

    const QString tableName = m_tableChooser->itemData( index ).toString();
    m_model->setTable( tableName );
    m_model->select();
}

void GtfsDatabaseTab::gtfsDatabaseStateChanged( Project::GtfsDatabaseState state )
{
    kDebug() << "GTFS state changed to" << state;
    switch ( state ) {
    case Project::GtfsDatabaseImportFinished:
        m_tableView->setModel( 0 );
        m_queryTableView->setModel( 0 );
        delete m_model;
        delete m_queryModel;
        m_model = new QSqlTableModel( this,
                GtfsDatabase::database(project()->data()->id()) );
        m_queryModel = new QSqlQueryModel( this );
        tableChosen( m_tableChooser->currentIndex() );
        m_tableView->setModel( m_model );
        m_queryTableView->setModel( m_queryModel );
        m_tabWidget->setTabEnabled( 1, true );
        m_tabWidget->setTabEnabled( 2, true );
        break;
    case Project::GtfsDatabaseError:
    case Project::GtfsDatabaseImportPending:
    case Project::GtfsDatabaseImportRunning:
        m_tabWidget->setTabEnabled( 1, false );
        m_tabWidget->setTabEnabled( 2, false );
        break;
    default:
        break;
    }
}
