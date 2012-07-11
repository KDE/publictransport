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
#include "projectsdockwidget.h"

// Own includes
#include "config.h"
#include "../projectmodel.h"
#include "../tabs/projectsourcetab.h"
#include "../project.h"
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    #include "../tabs/scripttab.h"
#endif

// KDE includes
#include <KLocalizedString>
#include <KIconLoader>
#include <KDebug>
#include <KIcon>
#include <KMenu>
#include <KFileDialog>

// Qt includes
#include <QLabel>
#include <QBoxLayout>
#include <QTreeView>
#include <QFormLayout>

ProjectsDockWidget::ProjectsDockWidget( ProjectModel *model, KActionMenu *showDocksAction,
                                        QWidget *parent )
        : AbstractDockWidget( i18nc("@window:title Dock title", "Projects"),
                             showDocksAction, parent ),
          m_model(model), m_projectsWidget(0)
{
    setObjectName( "projects" );

    setWhatsThis( i18nc("@info:whatsthis", "<title>Opened projects</title>"
                        "<para>Shows a list of all opened projects. For each project a set of "
                        "tabs can be opened using the child items. There are handy context menus "
                        "available.</para>"
                        "<para>One project is always the <interface>Active Project</interface> "
                        "(if at least one is loaded). Menu and toolbar actions get connected to "
                        "the active project and the other docks show data for or control the "
                        "active project.</para>") );

    QWidget *container = new QWidget( this );
    container->setMinimumSize( 150, 150 );
    QLabel *label = new QLabel( i18nc("@info", "Active Project:"), container );
    m_projectsWidget = new QTreeView( container );
    m_projectsWidget->setModel( model );
    m_projectsWidget->setHeaderHidden( true );
    m_projectsWidget->setIndentation( 10 );
    m_projectsWidget->setAnimated( true );
    m_projectsWidget->setExpandsOnDoubleClick( true );
    m_projectsWidget->setEditTriggers( QAbstractItemView::NoEditTriggers );
    m_projectsWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    QSizePolicy sizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    sizePolicy.setVerticalStretch( 1 );
    m_projectsWidget->setSizePolicy( sizePolicy );

    QFormLayout *projectsLayout = new QFormLayout( container );
    projectsLayout->setContentsMargins( 0, 0, 0, 0 );
    projectsLayout->setVerticalSpacing( 0 );
    projectsLayout->setRowWrapPolicy( QFormLayout::WrapLongRows );
    projectsLayout->addRow( m_projectsWidget );
    setWidget( container );
    connect( m_projectsWidget, SIGNAL(doubleClicked(QModelIndex)),
             this, SLOT(projectItemDoubleClicked(QModelIndex)) );
    connect( m_projectsWidget, SIGNAL(customContextMenuRequested(QPoint)),
             this, SLOT(projectItemContextMenuRequested(QPoint)) );
}

void ProjectsDockWidget::projectItemDoubleClicked( const QModelIndex &index )
{
    if ( !m_model->flags(index).testFlag(Qt::ItemIsEnabled) ) {
        // A disabled item was double clicked
        return;
    }

    ProjectModelItem *projectItem = m_model->projectItemFromIndex( index );
    Project *project = projectItem->project();
    if ( index.parent().isValid() ) {
        // Is a child of the project root
        switch ( projectItem->type() ) {
        case ProjectModelItem::DashboardItem:
            project->showDashboardTab( this );
            break;
        case ProjectModelItem::ProjectSourceItem:
            project->showProjectSourceTab( this );
            break;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        case ProjectModelItem::ScriptItem:
            project->showScriptTab( this );
            break;
#endif
        case ProjectModelItem::PlasmaPreviewItem:
            project->showPlasmaPreviewTab( this );
            break;
        case ProjectModelItem::WebItem:
            project->showWebTab( this );
            break;
        case ProjectModelItem::ProjectItem:
            // Project root item double clicked, do nothing
            break;
        default:
            kWarning() << "Project model item type unknown" << projectItem->type();
            return;
        }
    }
}

void ProjectsDockWidget::projectItemContextMenuRequested( const QPoint &pos )
{
    const QModelIndex index = m_projectsWidget->indexAt( pos );
    if ( !index.isValid() ) {
        return; // No item clicked
    }

    ProjectModelItem *projectItem = m_model->projectItemFromIndex( index );
    if ( index.parent().isValid() ) {
        // Show context menu for child item
        KMenu *projectMenu = new KMenu( this );
        const TabType tabType = ProjectModelItem::tabTypeFromProjectItemType( projectItem->type() );
        QAction *openInTabAction = 0;
        QAction *closeTabAction = 0;
        QAction *documentSaveAction = 0;
        QAction *openExternalAction = 0;
        ProjectSourceTab *projectSourceTab = projectItem->project()->projectSourceTab();
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        ScriptTab *scriptTab = projectItem->project()->scriptTab();
#else
        AbstractTab *scriptTab = 0; // Dummy for less #ifdefs
#endif

        // Add a tab/document title
        AbstractTab *tab = projectItem->project()->tab( tabType );
        if ( tab ) {
            projectMenu->addTitle( tab->icon(), tab->title() );
        } else {
            projectMenu->addTitle( m_model->data(index, Qt::DecorationRole).value<QIcon>(),
                                   m_model->data(index, Qt::DisplayRole).toString() );
        }

        // Add an action to open the associated tab / create the document and open it in a tab
        const bool tabOpened = projectItem->project()->isTabOpened( tabType );
        if ( !tabOpened ) {
            // Tab for the project item is not opened, add an open/create action
            openInTabAction = projectItem->project()->scriptFileName().isEmpty()
                    ? projectMenu->addAction(KIcon("document-new"),
                                             i18nc("@item:inmenu", "Create From Template"))
                    : projectMenu->addAction(KIcon("document-open"),
                                             i18nc("@item:inmenu", "Open in Tab"));
        }

        // Add an action to open external script files for the script item
        if ( projectItem->isScriptItem() ) {
            openExternalAction = projectMenu->addAction( KIcon("document-open"),
                    i18nc("@item:inmenu", "Open External Script") );
        }

        // Add a save action for document items
        if ( projectItem->isProjectSourceItem() || projectItem->isScriptItem() ) {
            documentSaveAction = projectMenu->addAction( KIcon("document-save"),
                                                         i18nc("@item:inmenu", "Save Document") );
            documentSaveAction->setEnabled(
                    (projectItem->isProjectSourceItem() && projectSourceTab && projectSourceTab->isModified()) ||
                    (projectItem->isScriptItem() && scriptTab && scriptTab->isModified()) );
        }

        // Add a tab close action
        if ( tabOpened ) {
            // Tab for the project item is already opened
            closeTabAction = projectMenu->addAction(KIcon("tab-close"),
                    i18nc("@item:inmenu", "Close Tab"));
        }

        // Add a title "Project" and context menu actions of the project
        projectMenu->addTitle( KIcon("project-development"),
                               i18nc("@title:menu In-menu title", "Project") );
        projectMenu->addActions( projectItem->project()->contextMenuActions(this) );

        // Show the context menu
        QAction *triggeredAction = projectMenu->exec( m_projectsWidget->mapToGlobal(pos) );
        delete projectMenu;

        if ( triggeredAction == openInTabAction && openInTabAction ) {
            // Open project item in a tab
            projectItemDoubleClicked( index );
        } else if ( triggeredAction == closeTabAction && closeTabAction ) {
            // Close the tab
            projectItem->project()->closeTab( tabType );
        } else if ( triggeredAction == documentSaveAction ) {
            // Save project item
            if ( projectItem->isProjectSourceItem() && projectSourceTab ) {
                projectSourceTab->save();
            } else if ( projectItem->isScriptItem() && scriptTab ) {
                scriptTab->save();
            }
        } else if ( triggeredAction == openExternalAction ) {
            // Open external script file (from the same directory)
            QPointer<KFileDialog> dialog( new KFileDialog(projectItem->project()->path(),
                                                          QString(), this) );
            dialog->setMimeFilter( QStringList() << "application/javascript" );
            if ( dialog->exec() == KFileDialog::Accepted ) {
                const QString filePath = dialog->selectedFile();
                // TODO Check if the directory was changed
                projectItem->project()->showExternalScriptTab( filePath );
            }
            delete dialog.data();
        }
    } else {
        // A project item was clicked, open the project context menu
        projectItem->project()->showProjectContextMenu( m_projectsWidget->mapToGlobal(pos) );
    }
}

