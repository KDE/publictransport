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

#ifndef LINKCHECKER_H
#define LINKCHECKER_H

// KDE includes
#include <KUrl>

// Qt includes
#include <QDateTime>

class QNetworkReply;

/** @brief Checks if a given URL is valid and points to an existing resource. */
class LinkChecker : public QObject {
    Q_OBJECT
public:
    /** @brief States of the link checker. */
    enum State {
        NotStarted = 0, /**< Not started, yet. Use start() to start the check. */
        GetHeader, /**< Is currently getting headers for the URL. */
        GetComplete, /**< Is currently downloading the URL completely. */
        Finished /**< Is finished, may be erroneous. */
    };

    /** @brief Error types. */
    enum Error {
        NoError = 0, /**< There was no error. */
        UrlIsInvalid, /**< The given URL is invalid. */
        LinkIsDead, /**< The URL points to a not existant resource. */
        ReplyWasEmpty /**< The reply was empty. */
    };

    LinkChecker( const KUrl &url );
    ~LinkChecker();

    void start();

    State state() const { return m_state; };
    Error error() const { return m_error; };
    bool hasError() const { return m_error != NoError; };
    QString errorString() const { return m_errorString; };
    bool isFinished() const { return m_state == Finished; };
    KUrl url() const { return m_url; };
    QString redirectedUrl() const { return m_lastRedirectUrl; };
    QDateTime lastModificationTime() const { return m_lastModification; };
    qulonglong size() const { return m_size; };

signals:
    void finished( const QDateTime &lastModification = QDateTime(), qulonglong size = -1,
                   Error error = NoError, const QString &errorString = QString() );

protected slots:
    void statLinkFinished( QNetworkReply *reply );
    void abortRequest();

private:
    State m_state;
    Error m_error;
    const KUrl m_url;
    QString m_lastRedirectUrl;
    QString m_errorString;
    QDateTime m_lastModification;
    qulonglong m_size;
    QNetworkReply *m_reply;
};

#endif // LINKCHECKER_H
