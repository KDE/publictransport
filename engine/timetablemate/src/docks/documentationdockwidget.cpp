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
#include "documentationdockwidget.h"

// KDE includes
#include <KComboBox>
#include <KWebView>
#include <KToolBar>
#include <KIcon>
#include <KStandardAction>
#include <KLocalizedString>
#include <KGlobal>
#include <KDebug>
#include <KStandardDirs>
#include <KAction>

// Qt includes
#include <QBoxLayout>
#include <QMouseEvent>

DocumentationDockWidget::DocumentationDockWidget( KActionMenu *showDocksAction, QWidget *parent )
        : AbstractDockWidget( i18nc("@window:title Dock title", "Documentation"),
                             showDocksAction, parent )
{
    setObjectName( "documentation" );

    setWhatsThis( i18nc("@info:whatsthis", "<title>Documentation</title>"
                        "<para>Provides documentation for Public Transport engine scripts.</para>") );

    QWidget *container = new QWidget( this );
    container->setMinimumSize( 150, 150 );
    m_documentationChooser = new KComboBox( container );
    KIcon classIcon( "code-class" );
    m_documentationChooser->addItem( KIcon("go-home"), "Documentation Home", "index" );
    m_documentationChooser->addItem( KIcon("code-variable"), "Enumerations", "enums" );
    m_documentationChooser->addItem( classIcon, "Helper Object", "helper" );
    m_documentationChooser->addItem( classIcon, "Result Object", "resultobject" );
    m_documentationChooser->addItem( classIcon, "Network Object", "network" );
    m_documentationChooser->addItem( classIcon, "NetworkRequest Objects", "networkrequest" );
    m_documentationChooser->addItem( classIcon, "Storage Object", "storage" );
    m_documentationWidget = new KWebView( container );
    m_documentationWidget->pageAction( QWebPage::OpenLinkInNewWindow )->setVisible( false );
    m_documentationWidget->pageAction( QWebPage::OpenFrameInNewWindow )->setVisible( false );
    m_documentationWidget->pageAction( QWebPage::OpenImageInNewWindow )->setVisible( false );
    KToolBar *toolBar = new KToolBar( "DocumentationToolBar", container );
    toolBar->setToolButtonStyle( Qt::ToolButtonIconOnly );
    toolBar->addAction( m_documentationWidget->pageAction(QWebPage::Back) );
    toolBar->addAction( m_documentationWidget->pageAction(QWebPage::Forward) );
    toolBar->addWidget( m_documentationChooser );
    connect( m_documentationWidget, SIGNAL(urlChanged(QUrl)),
             this, SLOT(documentationUrlChanged(QUrl)) );
    QVBoxLayout *dockLayout = new QVBoxLayout( container );
    dockLayout->setSpacing( 0 );
    dockLayout->setContentsMargins( 0, 0, 0, 0 );
    dockLayout->addWidget( toolBar );
    dockLayout->addWidget( m_documentationWidget );
    setWidget( container );
    connect( m_documentationChooser, SIGNAL(currentIndexChanged(int)),
             this, SLOT(documentationChosen(int)) );
}

void DocumentationDockWidget::showEvent( QShowEvent *event )
{
    if ( !m_documentationWidget->url().isValid() ) {
        // Load documentation HTML when the dock widget gets shown for the first time
        documentationChosen( 0 );
    }
    QWidget::showEvent( event );
}

void DocumentationDockWidget::documentationChosen( int index )
{
    const QString page = m_documentationChooser->itemData( index ).toString();
    const QString documentationFileName = KGlobal::dirs()->findResource(
            "data", QString("timetablemate/doc/%1.html").arg(page) );
    m_documentationWidget->load( QUrl("file://" + documentationFileName) );
}

void DocumentationDockWidget::documentationUrlChanged( const QUrl &url )
{
    QRegExp regExp("/doc/(.*)\\.html(?:#.*)?$");
    if ( regExp.indexIn(url.toString()) != -1 ) {
        const QString page = regExp.cap( 1 );
        const int index = m_documentationChooser->findData( page );
        if ( index != -1 ) {
            m_documentationChooser->setCurrentIndex( index );
        } else {
            kDebug() << "Documentation page not found:" << page;
        }
    } else {
        kDebug() << "Unexpected url format:" << url;
    }
}
