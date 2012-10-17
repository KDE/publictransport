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

#ifndef MARBLEPROCESS_H
#define MARBLEPROCESS_H

// Own includes
#include "publictransporthelper_export.h"

// KDE includes
#include <KProcess>

class PUBLICTRANSPORTHELPER_EXPORT MarbleProcess : public KProcess {
    Q_OBJECT
public:
    MarbleProcess( const QString &stopName, qreal longitude, qreal latitude, QObject *parent );

signals:
    void marbleError( const QString &errorMessage );

public slots:
    bool centerOnStop( const QString &stopName = QString(),
                       qreal longitude = 0.0, qreal latitude = 0.0 );
    void slotError( QProcess::ProcessError processError );

protected slots:
    void hasStarted();

private:
    QString m_stopName;
    qreal m_longitude;
    qreal m_latitude;
};

#endif // Multiple inclusion guard
