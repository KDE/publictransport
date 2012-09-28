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

#ifndef TABS_ENUMS_H
#define TABS_ENUMS_H

// Own includes
#include "config.h"

// Qt includes
#include <QObject>

class Tabs : public QObject {
    Q_ENUMS( TabType )

public:
    /** @brief Types of tabs. */
    enum Type {
        NoTab = 0, /**< No tab. */

        Dashboard = 1, /**< Dashboard tab. */
        ProjectSource = 2, /**< Project source document tab. */
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        Script = 3, /**< Script document tab. */
#endif
        Web = 4, /**< Web tab. */
        PlasmaPreview = 5 /**< Plasma preview tab. */
    };

    static QLatin1String nameForType( Type type ) {
        switch ( type ) {
        case Tabs::Dashboard:
            return QLatin1String("dashboard");
        case Tabs::ProjectSource:
            return QLatin1String("source");
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        case Tabs::Script:
            return QLatin1String("script");
#endif
        case Tabs::Web:
            return QLatin1String("web");
        case Tabs::PlasmaPreview:
            return QLatin1String("plasma");
        default:
            return QLatin1String("unknown");
        }
    };
};

typedef Tabs::Type TabType;

#endif // Multiple inclusion guard
