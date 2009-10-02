/*
*   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef APPLETWITHSTATE_HEADER
#define APPLETWITHSTATE_HEADER

#include <Plasma/PopupApplet>

#include "global.h"

class AppletWithState : public Plasma::PopupApplet {
    public:
	// Basic Create/Destroy
	AppletWithState(QObject *parent, const QVariantList &args) : Plasma::PopupApplet(parent, args) {};

	virtual bool testState( AppletState state ) const = 0;
	virtual void addState( AppletState state ) = 0;
	virtual void removeState( AppletState state ) = 0;
	virtual void unsetStates( QList<AppletState> states ) = 0;
};

#endif