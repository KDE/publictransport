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

#ifndef PLASMAPREVIEWTAB_H
#define PLASMAPREVIEWTAB_H

#include "abstracttab.h"

/** @brief Represents a plasma preview tab. */
class PlasmaPreviewTab : public AbstractTab {
    Q_OBJECT
public:
    static PlasmaPreviewTab *create( Project *project, QWidget *parent );
    virtual inline TabType type() const { return Tabs::PlasmaPreview; };

    PlasmaPreview *plasmaPreviewWidget() const;
//     void closePlasmaPreview() const;

protected slots:
    void plasmaPreviewLoaded();

private:
    PlasmaPreviewTab( Project *project, QWidget *parent = 0 );
};

#endif // Multiple inclusion guard
