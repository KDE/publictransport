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

#ifndef WEBTAB_H
#define WEBTAB_H

// Own includes
#include "abstracttab.h"

class NetworkMonitorModel;
class MonitorNetworkAccessManager;
class KWebView;
class KUrlComboBox;
class KUrl;
class KToolBar;
class QNetworkReply;
class QWebInspector;

/** @brief Represents a web tab. */
class WebTab : public AbstractTab {
    Q_OBJECT
public:
    virtual ~WebTab();

    static WebTab *create( Project *project, QWidget *parent );
    virtual inline TabType type() const { return Tabs::Web; };

    inline KWebView *webView() const { return m_webView; };
    inline QWebInspector *webInspector() const { return m_inspector; };
    inline KUrlComboBox *urlBar() const { return m_urlBar; };
    inline NetworkMonitorModel *networkMonitorModel() const { return m_networkMonitorModel; };

signals:
    void canGoBackChanged( bool canGoBack );
    void canGoForwardChanged( bool canGoForward );
    void canStopChanged( bool canStop );

protected slots:
    void urlChanged( const QUrl &url );
    void urlBarReturn( const QString &url );
    void urlActivated( const KUrl &url );
    void slotLoadStarted();
    void slotLoadFinished();
    void faviconChanged();

private:
    WebTab( Project *project, QWidget *parent = 0 );

    KWebView *m_webView;
    QWebInspector *m_inspector;
    KToolBar *m_toolBar;
    KUrlComboBox *m_urlBar;
    MonitorNetworkAccessManager *m_networkMonitor;
    NetworkMonitorModel *m_networkMonitorModel;
};

#endif // Multiple inclusion guard
