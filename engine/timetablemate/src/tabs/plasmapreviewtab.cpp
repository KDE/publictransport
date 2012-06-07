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
#include "plasmapreviewtab.h"

// Own includes
#include "project.h"
#include "plasmapreview.h"

// Public Transport engine includes
#include <engine/timetableaccessor.h>
#include <engine/timetableaccessor_info.h>

PlasmaPreviewTab::PlasmaPreviewTab( Project *project, QWidget *parent )
        : AbstractTab(project, type(), parent)
{
    // Create plasma preview widget
    PlasmaPreview *plasmaPreviewWidget = new PlasmaPreview( parent );
    setWidget( plasmaPreviewWidget );

    plasmaPreviewWidget->setWhatsThis( i18nc("@info:whatsthis",
            "<subtitle>Plasma Preview</subtitle>"
            "<para>This is a preview of the PublicTransport applet in a plasma desktop. "
            "The applet's settings are changed so that it always uses the currently opened "
            "timetable accessor.</para>"
            "<para><note>You have to install the accessor to use it in this preview. "
            "Use <interface>File -&gt; Install</interface> to install the accessor locally "
            "or <interface>File -&gt; Install Globally</interface> to install the accessor "
            "globally, ie. for all users.</note></para>") );
    connect( plasmaPreviewWidget, SIGNAL(plasmaPreviewLoaded()),
             this, SLOT(plasmaPreviewLoaded()) );
}

PlasmaPreviewTab *PlasmaPreviewTab::create( Project *project, QWidget *parent )
{
    return new PlasmaPreviewTab( project, parent );
}

PlasmaPreview *PlasmaPreviewTab::plasmaPreviewWidget() const
{
    return qobject_cast<PlasmaPreview *>( widget() );
}

// void PlasmaPreviewTab::closePlasmaPreview() const
// {
//     plasmaPreviewWidget()->closePlasmaPreview();
// }

void PlasmaPreviewTab::plasmaPreviewLoaded()
{
    plasmaPreviewWidget()->setSettings( project()->accessor()->info()->serviceProvider(),
                                        QString() );
}
