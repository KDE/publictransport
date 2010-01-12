/*
*   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef ALARMTIMER_HEADER
#define ALARMTIMER_HEADER

// Qt includes
#include <QTimer>
#include <QPersistentModelIndex>
#include <QDateTime>

/** @class AlarmTimer
* @brief Stores the QPersistantModelIndex of the item to that the alarm belongs. */
class AlarmTimer : public QObject {
    Q_OBJECT

    public:
	AlarmTimer( QTimer *timer, QPersistentModelIndex data = QPersistentModelIndex() ) {
	    setData( data );
	    setTimer( timer );
	};
	AlarmTimer( int msecs, QPersistentModelIndex data = QPersistentModelIndex(), bool singleShot = true ) {
	    setupSingleShotTimer( msecs, data, singleShot );
	};

	QPersistentModelIndex data() const {
	    return m_data;
	};
	void setData( const QPersistentModelIndex &data ) {
	    m_data = data;
	}
	QTimer *timer() const {
	    return m_timer;
	};
	void setTimer( QTimer *timer );
	QTimer *setupSingleShotTimer( int msecs, QPersistentModelIndex data = QPersistentModelIndex(), bool singleShot = true );

	QDateTime startedAt() { return m_startedAt; };

    public slots:
	void timeoutReceived();

    signals:
	void timeout( const QPersistentModelIndex &modelIndex );

    private:
	QTimer *m_timer;
	QDateTime m_startedAt;
	QPersistentModelIndex m_data;
};

#endif // ALARMTIMER_HEADER