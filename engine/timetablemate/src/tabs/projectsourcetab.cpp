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
#include "projectsourcetab.h"

// Own includes
#include "project.h"

// KDE includes
#include <KLocalizedString>
#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/ConfigInterface>
#include <KFileDialog>
#include <KGlobalSettings>

// Qt includes
#include <QWidget>

ProjectSourceTab::ProjectSourceTab( Project *project, KTextEditor::Document *document, QWidget *parent )
        : AbstractDocumentTab( project, document, Tabs::ProjectSource, parent )
{
}

ProjectSourceTab *ProjectSourceTab::create( Project *project, QWidget *parent )
{
    // Create script document
    QWidget *container = new QWidget( parent );
    KTextEditor::Document *document = createDocument( container );
    if ( !document ) {
        delete container;
        return 0;
    }

    // Create tab
    ProjectSourceTab *tab = new ProjectSourceTab( project, document, parent );
    document->setHighlightingMode( "XML" );

    KTextEditor::View *view = tab->defaultView();
    view->setWhatsThis( i18nc("@info:whatsthis", "<subtitle>Project Source</subtitle>"
            "<para>This shows the XML source of the project. Normally you will not need this, "
            "because you can setup everything in the <interface>Project Settings</interface>.</para>") );

    KTextEditor::ConfigInterface *configInterface =
            qobject_cast<KTextEditor::ConfigInterface*>( view );
    if ( configInterface ) {
        configInterface->setConfigValue( "dynamic-word-wrap", true );
    }

    tab->setWidget( view );
    return tab;
}

bool ProjectSourceTab::save()
{
    kDebug() << "Modified?" << isModified() << document()->isModified();
    if ( !isModified() ) {
        return true;
    }

    QString fileName = project()->filePath();
    if ( fileName.isEmpty() ) {
        fileName = KFileDialog::getSaveFileName( KGlobalSettings::documentPath(),
                "application/x-publictransport-serviceprovider application/xml",
                this, i18nc("@title:window", "Save ") );
        if ( fileName.isNull() ) {
            return false; // Cancel clicked
        }
    }

    document()->saveAs( fileName );
    kDebug() << "Document saved";
    return true;
}
