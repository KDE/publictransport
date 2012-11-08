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
#include "linkchecker.h"

// KDE includes
#include <KDebug>
#include <KLocalizedString>

// Qt includes
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

LinkChecker::LinkChecker( const KUrl &url )
        : m_state(NotStarted), m_error(NoError), m_url(url), m_size(-1), m_reply(0)
{
}

LinkChecker::~LinkChecker()
{
}

void LinkChecker::start()
{
    if ( !m_url.isValid() ) {
        m_error = UrlIsInvalid;
        m_errorString = i18nc("@info", "The URL is invalid");
        emit finished( QDateTime(), -1, m_error, m_errorString );
        return;
    }

    QNetworkAccessManager *manager = new QNetworkAccessManager( this );
    connect( manager, SIGNAL(finished(QNetworkReply*)),
             this, SLOT(statLinkFinished(QNetworkReply*)) );
    QNetworkRequest request( m_url );
    m_reply = manager->head( request );
    m_state = GetHeader;
}

void LinkChecker::statLinkFinished( QNetworkReply *reply )
{
    Q_ASSERT( m_state == GetHeader || m_state == GetComplete );

    // Check if there is a valid redirection
    QString redirectUrl = reply->attribute( QNetworkRequest::RedirectionTargetAttribute ).toString();
    if ( reply->error() == QNetworkReply::OperationCanceledError ) {
        // Was canceled, but should be ok, because it has started downloading without errors,
        // but we do not want to wait for the download to finish
        m_errorString = i18nc("@info/plain", "Seems to be ok, could not get only the header values for the URL");
    } else if ( reply->error() != QNetworkReply::NoError ) {
        // GTFS feed not available
        if ( m_state != GetComplete ) {
            m_state = GetComplete;
            m_reply = reply->manager()->get( QNetworkRequest(m_url) );
            reply->deleteLater();
            return;
        }
        m_error = LinkIsDead;
        m_errorString = reply->errorString();
    } else if( !redirectUrl.isEmpty() && redirectUrl != m_lastRedirectUrl ) {
        // Redirect to redirectUrl
        m_lastRedirectUrl = redirectUrl;
        m_reply = reply->manager()->head( QNetworkRequest(redirectUrl) );
        reply->deleteLater();
        return;
    } else if ( reply->header(QNetworkRequest::ContentLengthHeader).toInt() == 0 &&
                m_state != GetComplete )
    {
        // Got no size information when only requesting the header, download completely document now
        m_state = GetComplete;
        m_reply = reply->manager()->get( QNetworkRequest(m_url) );
        QTimer::singleShot( 1000, this, SLOT(abortRequest()) );
        reply->deleteLater();
        return;
    } else {
        // No error or redirection, read reply headers
        m_lastModification = reply->header( QNetworkRequest::LastModifiedHeader ).toDateTime();
        m_size = reply->header( QNetworkRequest::ContentLengthHeader ).toULongLong();

        if ( m_size == 0 && m_state == GetComplete ) {
            // Size not found in reply headers, get size from the completely received data
            m_size = reply->size();
        }

        if ( m_size == 0 ) {
            // Document is empty
            m_error = ReplyWasEmpty;
            m_errorString = i18nc("@info/plain", "Empty reply");
        }
    }
    m_reply = 0;
    reply->deleteLater();

    m_state = Finished;
    emit finished( m_lastModification, m_size, m_error, m_errorString );
}

void LinkChecker::abortRequest()
{
    if ( !m_reply ) {
        // Already finished
        return;
    }

    m_reply->abort();
}
