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
#include "dashboardtab.h"
#include "../project.h"

// PublicTransport engine includes
#include <engine/serviceprovider.h>
#include <engine/serviceproviderdata.h>

// KDE includes
#include <KStandardDirs>
#include <KDebug>
#include <kdeclarative.h>

// Qt includes
#include <QDeclarativeView>
#include <qdeclarative.h>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QBoxLayout>
#include <QToolButton>
#include <QAction>
#include <QGraphicsEffect>
#include <QContextMenuEvent>

DashboardTab::DashboardTab( Project *project, QWidget *parent )
        : AbstractTab(project, type(), parent), m_qmlView(0), m_project(project)
{
    // Find the QML file used for the dashboard tab
    const QString fileName = KGlobal::dirs()->findResource( "data", "timetablemate/dashboard.qml" );
    if ( fileName.isEmpty() ) {
        kWarning() << "dashboard.qml not found! Check installation";
        return;
    }
    const QString svgFileName = KGlobal::dirs()->findResource( "data", "timetablemate/dashboard.svg" );

    // Register classes in Qt's meta object system and for QML
    qRegisterMetaType< const ServiceProviderData* >( "const ServiceProviderData*" );
    qRegisterMetaType< Project* >( "Project*" );
    qRegisterMetaType< TestModel* >( "TestModel*" );
    qRegisterMetaType< Enums::ServiceProviderType >( "Enums::ServiceProviderType" );
    qmlRegisterType< ServiceProviderData, 1 >( "TimetableMate", 1, 0, "ServiceProviderData" );
    qmlRegisterType< Project, 1 >( "TimetableMate", 1, 0, "Project" );
    qmlRegisterType< Tabs, 1 >( "TimetableMate", 1, 0, "Tabs" );
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

    m_qmlView->setResizeMode( QDeclarativeView::SizeRootObjectToView );
    m_qmlView->rootContext()->setContextProperty( "project", project );
    m_qmlView->rootContext()->setContextProperty( "svgFileName", svgFileName );

    // Add Plasma QML import paths
    const QStringList importPaths = KGlobal::dirs()->findDirs( "module", "imports" );
    foreach( const QString &importPath, importPaths ) {
        m_qmlView->engine()->addImportPath( importPath );
    }

    m_qmlView->setSource( fileName );
    QVBoxLayout *layout = new QVBoxLayout( container );
    layout->addWidget( m_qmlView );
    setWidget( container );
}

DashboardTab *DashboardTab::create( Project *project, QWidget *parent )
{
    DashboardTab *tab = new DashboardTab( project, parent );
    return tab;
}

void DashboardTab::contextMenuEvent( QContextMenuEvent *event )
{
    m_project->showProjectContextMenu( event->globalPos() );
}
