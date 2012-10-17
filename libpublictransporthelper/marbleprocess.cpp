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
#include "marbleprocess.h"

// KDE includes
#include <KMessageBox>
#include <KLocalizedString>
#include <KDebug>

// Qt includes
#include <QDBusMessage>
#include <QDBusConnection>
#include <QTimer>

MarbleProcess::MarbleProcess( const QString &stopName, qreal longitude, qreal latitude,
                              QObject *parent )
        : KProcess(parent), m_stopName(stopName), m_longitude(longitude), m_latitude(latitude)
{
    setProgram( "marble", QStringList() << "--caption"
            << i18nc("@title:window Caption for "
            "marble windows started to show a stops position in a map. %1 is the "
            "stop name.", "\"PublicTransport: %1\"", stopName) );
    connect( this, SIGNAL(error(QProcess::ProcessError)),
             this, SLOT(slotError(QProcess::ProcessError)) );
    connect( this, SIGNAL(started()), this, SLOT(hasStarted()) );
//     connect( d->marble, SIGNAL(finished(int)), this, SLOT(marbleFinished(int)) );
}

void MarbleProcess::hasStarted()
{
    // Wait for output from marble
    for ( int i = 0; i < 10; ++i ) {
        if ( waitForReadyRead(50) ) {
            break;
        }
    }

    QTimer::singleShot( 250, this, SLOT(centerOnStop()) );
}

void MarbleProcess::slotError( QProcess::ProcessError processError )
{
    if ( processError == QProcess::FailedToStart ) {
        int result = KMessageBox::questionYesNo( 0, i18nc("@info", "The map application "
                "'marble' couldn't be started, error message: <message>%1</message>.<nl/>"
                "Do you want to install 'marble' now?", errorString()) );
        if ( result == KMessageBox::Yes ) {
            // Start KPackageKit to install marble
            KProcess *kPackageKit = new KProcess( this );
            kPackageKit->setProgram( "kpackagekit",
                                     QStringList() << "--install-package-name" << "marble" );
            kPackageKit->start();
        }
    } else if ( processError == QProcess::Crashed ) {
        KMessageBox::sorry( 0, i18nc("@info", "The map application 'marble' crashed") );
    }
}

bool MarbleProcess::centerOnStop( const QString &_stopName, qreal _longitude, qreal _latitude )
{
    QString destination = QString("org.kde.marble-%1").arg(pid());

    QString stopName = _stopName;
    qreal longitude = _longitude;
    qreal latitude = _latitude;
    if ( stopName.isEmpty() ) {
        stopName = m_stopName;
        longitude = m_longitude;
        latitude = m_latitude;
    }

    // Set new window title
    if ( !stopName.isEmpty() ) {
        QDBusMessage m0 = QDBusMessage::createMethodCall(destination,
                "/marble/MainWindow_1", "org.kde.marble.KMainWindow", "setPlainCaption");
        m0 << i18nc("@title:window Caption for marble windows started to show a stops "
                "position in a map. %1 is the stop name.", "\"PublicTransport: %1\"",
                stopName);
        if ( !QDBusConnection::sessionBus().send(m0) ) {
            kDebug() << "Couldn't set marble title with dbus" << m0.errorMessage();
        }
    }

    // Load OpenStreetMap
    QDBusMessage m1 = QDBusMessage::createMethodCall(destination,
            "/MarbleMap", "org.kde.MarbleMap", "setMapThemeId");
    m1 << "earth/openstreetmap/openstreetmap.dgml";
    if ( !QDBusConnection::sessionBus().send(m1) ) {
        emit marbleError( i18nc("@info", "Couldn't interact with 'marble' (DBus: %1).",
                                m1.errorMessage()) );
        return false;
    }

    // Center on the stops coordinates
    QDBusMessage m2 = QDBusMessage::createMethodCall(destination,
            "/MarbleMap", "org.kde.MarbleMap", "centerOn");
    m2 << longitude << latitude;
    if ( !QDBusConnection::sessionBus().send(m2) ) {
        emit marbleError( i18nc("@info", "Couldn't interact with 'marble' (DBus: %1).",
                                m2.errorMessage()) );
        return false;
    }

    // Set zoom factor
    QDBusMessage m3 = QDBusMessage::createMethodCall(destination,
            "/MarbleWidget", "org.kde.MarbleWidget", "zoomView");
    m3 << 3080;
    if ( !QDBusConnection::sessionBus().send(m3) ) {
        emit marbleError( i18nc("@info", "Couldn't interact with 'marble' (DBus: %1).",
                                m3.errorMessage()) );
        return false;
    }

    // Update map
    QDBusMessage m4 = QDBusMessage::createMethodCall(destination,
            "/MarbleMap", "org.kde.MarbleMap", "reload");
    if ( !QDBusConnection::sessionBus().send(m4) ) {
        emit marbleError( i18nc("@info", "Couldn't interact with 'marble' (DBus: %1).",
                                m4.errorMessage()) );
        return false;
    }

    // All messages sent successfully
    return true;
}
