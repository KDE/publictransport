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
#include "webtab.h"

// Own includes
#include "project.h"
#include "../networkmonitormodel.h"

// KDE includes
#include <KUrlComboBox>
#include <KWebView>
#include <KLocalizedString>
#include <KToolBar>
#include <KGlobal>
#include <KStandardDirs>
#include <KDebug>

// Qt includes
#include <QWidget>
#include <QVBoxLayout>
#include <QAction>
#include <QSplitter>
#include <QWebHistory>
#include <QWebInspector>
#include <QWebFrame>
#include <QWebElement>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QTimer>
#include <QBuffer>

WebTab::WebTab( Project *project, QWidget *parent )
        : AbstractTab(project, type(), parent), m_webView(0), m_inspector(0), m_toolBar(0), m_urlBar(0),
          m_networkMonitor(new MonitorNetworkAccessManager(this)),
          m_networkMonitorModel(new NetworkMonitorModel(this))
{
    // Create web widget
    QWidget *container = new QWidget( parent );
    setWidget( container );

    // Create web view widget
    m_webView = new KWebView( container );
    NetworkMemoryCache *cache = new NetworkMemoryCache( this );
    m_networkMonitor->setCache( cache );
    m_webView->page()->setNetworkAccessManager( m_networkMonitor );

    m_webView->settings()->setAttribute( QWebSettings::DeveloperExtrasEnabled, true );
    m_webView->pageAction( QWebPage::OpenLinkInNewWindow )->setVisible( false );
    m_webView->pageAction( QWebPage::OpenFrameInNewWindow )->setVisible( false );
    m_webView->pageAction( QWebPage::OpenImageInNewWindow )->setVisible( false );
    m_webView->setMinimumHeight( 100 );
    m_webView->setWhatsThis( i18nc("@info:whatsthis", "<subtitle>Web View</subtitle>"
            "<para>This is the web view. You can use it to check the URLs you have defined "
            "in the <interface>Project Settings</interface> or to get information about the "
            "structure of the documents that get parsed by the script.</para>"
            "<para><note>You can select a web element in the <emphasis>inspector</emphasis> "
            "using the context menu.</note></para>") );
    m_webView->settings()->setIconDatabasePath(
            KGlobal::dirs()->saveLocation("data", "plasma_engine_publictransport") );

    // Create a web inspector
    m_inspector = new QWebInspector( container );
    m_inspector->setPage( m_webView->page() );
    m_inspector->setMinimumSize( 150, 150 );
    m_inspector->hide();

    // Connect network monitor model
    connect( m_networkMonitor, SIGNAL(requestCreated(NetworkMonitorModelItem::Type,QString,QByteArray,QNetworkReply*)),
             m_networkMonitorModel, SLOT(requestCreated(NetworkMonitorModelItem::Type,QString,QByteArray,QNetworkReply*)) );

    // Create URL bar
    m_urlBar = new KUrlComboBox( KUrlComboBox::Both, true, container );
    m_urlBar->setFont( KGlobalSettings::generalFont() ); // Do not use toolbar font as child widget of a KToolBar
    connect( m_webView, SIGNAL(statusBarMessage(QString)),
             this, SIGNAL(statusBarMessage(QString)) );
    connect( m_webView, SIGNAL(urlChanged(QUrl)),
             this, SLOT(urlChanged(QUrl)) );
    connect( m_webView, SIGNAL(iconChanged()),
             this, SLOT(faviconChanged()) );
    connect( m_urlBar, SIGNAL(returnPressed(QString)),
             this, SLOT(urlBarReturn(QString)) );
    connect( m_urlBar, SIGNAL(urlActivated(KUrl)),
             this, SLOT(urlActivated(KUrl)) );
    connect( m_webView, SIGNAL(loadStarted()), this, SLOT(slotLoadStarted()) );
    connect( m_webView, SIGNAL(loadFinished(bool)), this, SLOT(slotLoadFinished()) );

    // Create web toolbar
    m_toolBar = new KToolBar( "webToolBar", this, false );
    m_toolBar->addAction( m_webView->pageAction(QWebPage::Back) );
    m_toolBar->addAction( m_webView->pageAction(QWebPage::Forward) );
    m_toolBar->addAction( m_webView->pageAction(QWebPage::Stop) );
    m_toolBar->addAction( m_webView->pageAction(QWebPage::Reload) );
    m_toolBar->addWidget( m_urlBar );

    QVBoxLayout *layoutWeb = new QVBoxLayout( container );
    layoutWeb->setContentsMargins( 0, 2, 0, 0 ); // 2px margin at top
    layoutWeb->setSpacing( 0 );
    layoutWeb->addWidget( m_toolBar );
    layoutWeb->addWidget( m_webView );
}

void WebTab::slotLoadStarted()
{
    emit canStopChanged( true );
}

void WebTab::slotLoadFinished()
{
    emit canStopChanged( false );
}

WebTab *WebTab::create( Project *project, QWidget *parent )
{
    WebTab *tab = new WebTab( project, parent );
    return tab;
}

void WebTab::faviconChanged()
{
    const int index = m_urlBar->currentIndex();
    if ( index != -1 ) {
        m_urlBar->changeUrl( index, m_webView->icon(), m_webView->url() );
    } else {
        m_urlBar->addUrl( m_webView->icon(), m_webView->url() );
    }
}

void WebTab::urlChanged( const QUrl &url )
{
    if ( !m_urlBar->contains(url.toString()) ) {
        m_urlBar->insertUrl( 0, m_webView->icon(), url );
        m_urlBar->setCurrentIndex( 0 );
    } else {
        int index = -1;
        for ( int i = 0; i < m_urlBar->urls().count(); ++i ) {
            if ( m_urlBar->urls()[i] == url.toString() ) {
                index = i;
                break;
            }
        }
        if ( index == -1 ) {
            m_urlBar->setEditUrl( url );
        } else {
            m_urlBar->setCurrentIndex( index );
        }
    }

    emit canGoBackChanged( m_webView->history()->canGoBack() );
    emit canGoForwardChanged( m_webView->history()->canGoForward() );
}

void WebTab::urlBarReturn( const QString &url )
{
    m_webView->setUrl( KUrl::fromUserInput(url) );
}

void WebTab::urlActivated( const KUrl &url )
{
    m_webView->setUrl( url );
}
