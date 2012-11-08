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
#include <KLineEdit>

// Qt includes
#include <QLabel>
#include <QBoxLayout>
#include <QTreeView>
#include <QFormLayout>
#include <QSortFilterProxyModel>

ProjectsDockWidget::ProjectsDockWidget( ProjectModel *model, KActionMenu *showDocksAction,
                                        QWidget *parent )
        : AbstractDockWidget( i18nc("@window:title Dock title", "Projects"),
                             showDocksAction, parent ),
          m_model(model), m_projectsWidget(0), m_searchLine(0),
          m_proxyModel(new QSortFilterProxyModel(this))
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

    m_searchLine = new KLineEdit( container );
    m_searchLine->setClickMessage(
            i18nc("@info/plain A KLineEdit click message to filter projects", "Type to search") );
    m_searchLine->setClearButtonShown( true );

    m_proxyModel->setSourceModel( model );
    m_proxyModel->setFilterCaseSensitivity( Qt::CaseInsensitive );
    connect( m_searchLine, SIGNAL(textChanged(QString)),
             m_proxyModel, SLOT(setFilterFixedString(QString)) );

    m_projectsWidget = new QTreeView( container );
    m_projectsWidget->setModel( m_proxyModel );
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
    projectsLayout->addRow( m_searchLine );
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
        case ProjectModelItem::IncludedScriptItem: {
            ProjectModelIncludedScriptItem *includedScriptItem =
                    dynamic_cast< ProjectModelIncludedScriptItem* >( projectItem );
            project->showExternalScriptTab( includedScriptItem->filePath(), this );
            break;
        }
        case ProjectModelItem::CodeItem: {
            ProjectModelCodeItem *codeItem = dynamic_cast< ProjectModelCodeItem* >( projectItem );
            ProjectModelIncludedScriptItem *includedScriptItem = // TODO
                    dynamic_cast< ProjectModelIncludedScriptItem* >( codeItem->parent() );
            if ( includedScriptItem ) {
                Q_ASSERT( !includedScriptItem->filePath().isEmpty() );
            }
            ScriptTab *scriptTab = includedScriptItem
                    ? project->showExternalScriptTab(includedScriptItem->filePath(), this)
                    : project->showScriptTab(this);
            Q_ASSERT( scriptTab );
            scriptTab->goToLine( codeItem->node()->line() );
            break;
        }
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
    Project *project = projectItem->project();
    if ( index.parent().isValid() ) {
        // Show context menu for child item
        KMenu *projectMenu = new KMenu( this );
        const TabType tabType = ProjectModelItem::tabTypeFromProjectItemType( projectItem->type() );
        QAction *openInTabAction = 0;
        QAction *showCodeNodeAction = 0;
        QAction *closeTabAction = 0;
        QAction *documentSaveAction = 0;
        ProjectSourceTab *projectSourceTab = project->projectSourceTab();
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        ProjectModelCodeItem *codeItem = dynamic_cast< ProjectModelCodeItem* >( projectItem );
        ProjectModelIncludedScriptItem *includedScriptItem =
                dynamic_cast< ProjectModelIncludedScriptItem* >( projectItem );
        ScriptTab *scriptTab = projectItem->isIncludedScriptItem()
                ? project->scriptTab(includedScriptItem->filePath()) : project->scriptTab();
        AbstractTab *tab = includedScriptItem ? project->scriptTab(includedScriptItem->filePath())
                                              : project->tab(tabType);
#else
        AbstractTab *scriptTab = 0; // Dummy for less #ifdefs
        bool includedScriptItem = false;
        AbstractTab *tab = project->tab( tabType );
#endif


        if ( tab ) {
            projectMenu->addTitle( tab->icon(), tab->title() );
        } else {
            projectMenu->addTitle( m_model->data(index, Qt::DecorationRole).value<QIcon>(),
                                   m_model->data(index, Qt::DisplayRole).toString() );
        }

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        // Add an action to open the associated tab / create the document and open it in a tab
        if ( codeItem ) {
            showCodeNodeAction = projectMenu->addAction( KIcon("arrow-right"),
                                           i18nc("@item:inmenu", "Show in Script Tab"));
        } else
#endif
        {
            const bool tabOpened = includedScriptItem
                    ? bool(scriptTab) : project->isTabOpened(tabType);
            if ( !tabOpened ) {
                // Tab for the project item is not opened, add an open/create action
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
                const QString fileName = includedScriptItem
                        ? includedScriptItem->filePath() : project->scriptFileName();
#else
                const QString fileName = project->scriptFileName();
#endif
                openInTabAction = fileName.isEmpty()
                        ? projectMenu->addAction(KIcon("document-new"),
                                                 i18nc("@item:inmenu", "Create From Template"))
                        : projectMenu->addAction(KIcon("document-open"),
                                                 i18nc("@item:inmenu", "Open in Tab"));
            }

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
            // Add an action to open external script files for the script item
            if ( projectItem->isScriptItem() ) {
                QAction *openExternalAction = project->projectAction( Project::ShowExternalScript );
                projectMenu->addAction( openExternalAction );
            }
#endif

            // Add a save action for document items
            if ( projectItem->isProjectSourceItem() || projectItem->isScriptItem() ||
                 projectItem->isIncludedScriptItem() )
            {
                documentSaveAction = projectMenu->addAction( KIcon("document-save"),
                                                            i18nc("@item:inmenu", "Save Document") );
                documentSaveAction->setEnabled(
                        (projectItem->isProjectSourceItem() && projectSourceTab && projectSourceTab->isModified()) ||
                        ((projectItem->isScriptItem() || projectItem->isIncludedScriptItem()) &&
                         scriptTab && scriptTab->isModified()) );
            }

            // Add a tab close action
            if ( tabOpened ) {
                // Tab for the project item is already opened
                closeTabAction = projectMenu->addAction(KIcon("tab-close"),
                        i18nc("@item:inmenu", "Close Tab"));
            }
        }

        // Add a title "Project" and context menu actions of the project
        projectMenu->addTitle( KIcon("project-development"),
                               i18nc("@title:menu In-menu title", "Project") );
        projectMenu->addActions( project->contextMenuActions(this) );

        // Show the context menu
        QAction *triggeredAction = projectMenu->exec( m_projectsWidget->mapToGlobal(pos) );
        delete projectMenu;

        if ( triggeredAction == 0 ) {
            return;
        } else if ( triggeredAction == openInTabAction ) {
            // Open project item in a tab
            projectItemDoubleClicked( index );
        } else if ( triggeredAction == closeTabAction ) {
            // Close the tab
            project->closeTab( tab );
        } else if ( triggeredAction == documentSaveAction ) {
            // Save project item
            if ( projectItem->isProjectSourceItem() && projectSourceTab ) {
                projectSourceTab->save();
            } else if ( scriptTab &&
                        (projectItem->isScriptItem() || projectItem->isIncludedScriptItem()) )
            {
                scriptTab->save();
            }
        }
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        else if ( triggeredAction == showCodeNodeAction ) {
            ScriptTab *scriptTab = projectItem->isIncludedScriptItem()
                    ? project->showExternalScriptTab(includedScriptItem->filePath(), this)
                    : project->showScriptTab( this );
            scriptTab->goToLine( codeItem->node()->line() );
        }
#endif
    } else {
        // A project item was clicked, open the project context menu
        project->showProjectContextMenu( m_projectsWidget->mapToGlobal(pos) );
    }
}

