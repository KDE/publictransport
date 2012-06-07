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
#include "projectmodel.h"

// Own includes
#include "project.h"
#include "tabs/scripttab.h"
#include "tabs/projectsourcetab.h"
#include "debugger/debugger.h"

// Public Transport engine includes
#include <engine/timetableaccessor.h>
#include <engine/timetableaccessor_info.h>

// KDE includes
#include <KLocalizedString>
#include <KIcon>
#include <KGlobalSettings>
#include <KDebug>
#include <KDialog>

// Qt includes
#include <QFileInfo>
#include <QFont>

ProjectModel::ProjectModel( QObject *parent ) : QAbstractItemModel( parent ), m_activeProject(0)
{
}

ProjectModel::~ProjectModel()
{
    clear();
}

void ProjectModel::setActiveProject( Project *activeProject )
{
    if ( activeProject == m_activeProject ) {
        return;
    }

    Project *previousProject = m_activeProject;
    emit activeProjectAboutToChange( activeProject, previousProject );
    if ( previousProject ) {
        const QModelIndex index = indexFromProject( previousProject );
        m_activeProject = activeProject;

        // Notify that the current debug project is no longer the debug project
        dataChanged( index, index );
    } else {
        m_activeProject = activeProject;
    }
    emit activeProjectChanged( activeProject, previousProject );

    // Notify about the new active project
    const QModelIndex index = indexFromProject( m_activeProject );
    dataChanged( index, index );
}

QModelIndex ProjectModel::index( int row, int column, const QModelIndex &parent ) const
{
    if ( parent.isValid() ) {
        if ( parent.column() != 0 ) {
            // Only the first column has children
            return QModelIndex();
        }

        // Child item
        ProjectModelItem *parentItem = projectItemFromIndex( parent );
        if ( row >= 0 && row < parentItem->children().count() ) {
            // Valid row for the child
            return createIndex( row, column, parentItem->children()[row] );
        }
    } else if ( row >= 0 && row < m_projects.count() ) {
        // Top level item
        return createIndex( row, column, m_projects[row] );
    }

    return QModelIndex();
}

int ProjectModel::rowCount( const QModelIndex &parent ) const
{
    if ( parent.isValid() ) {
        // Child item
        if ( parent.column() == 0 ) {
            ProjectModelItem *projectItem = projectItemFromIndex( parent );
            return projectItem->children().count();
        } else {
            return 0;
        }
    } else {
        // Top level item
        return m_projects.count();
    }
}

QModelIndex ProjectModel::parent( const QModelIndex &child ) const
{
    if ( !child.isValid() ) {
        return QModelIndex();
    }

    ProjectModelItem *projectItem = projectItemFromIndex( child );
    if ( projectItem->parent() ) {
        return indexFromProjectItem( projectItem->parent() );
    } else {
        return QModelIndex();
    }
}

QString ProjectModelItem::text() const
{
    switch ( m_type ) {
    case ProjectModelItem::OverviewItem:
        return i18nc("@info/plain", "Overview");
    case ProjectModelItem::ScriptItem: {
        QString name = m_project->scriptFileName().isEmpty()
                ? i18nc("@info/plain", "Script File")
                : QFileInfo(m_project->scriptFileName()).fileName();
        return KDialog::makeStandardCaption( name, 0,
                m_project->scriptTab() && m_project->scriptTab()->isModified()
                ? KDialog::ModifiedCaption : KDialog::NoCaptionFlags );
    }
    case ProjectModelItem::AccessorItem: {
        QString name = m_project->filePath().isEmpty()
                ? i18nc("@info/plain", "Project Source XML File (experts)")
                : i18nc("@info/plain", "%1 (experts)",
                        QFileInfo(m_project->filePath()).fileName());
        return KDialog::makeStandardCaption( name, 0, m_project->isProjectSourceModified()
                ? KDialog::ModifiedCaption : KDialog::NoCaptionFlags );
    }
    case ProjectModelItem::PlasmaPreviewItem:
        return i18nc("@info/plain", "Plasma Preview");
    case ProjectModelItem::WebItem:
        return i18nc("@info/plain", "Service Provider Home Page");
    default:
        kWarning() << "Unknown project item type" << m_type;
        return QString();
    }
}

QVariant ProjectModel::data( const QModelIndex &index, int role ) const
{
    ProjectModelItem *projectItem = static_cast< ProjectModelItem* >( index.internalPointer() );
    Project *project = projectItem->project();

    if ( index.parent().isValid() ) {
        // Child item of a project item
        switch ( role ) {
        case Qt::DisplayRole:
            return projectItem->text();
        case Qt::DecorationRole:
            switch ( projectItem->type() ) {
            case ProjectModelItem::OverviewItem:
                return KIcon("zoom-draw");
            case ProjectModelItem::AccessorItem:
                return KIcon("application-x-publictransport-serviceprovider");
            case ProjectModelItem::ScriptItem:
                return project->scriptIcon();
            case ProjectModelItem::PlasmaPreviewItem:
                return KIcon("plasma");
            case ProjectModelItem::WebItem:
                return KIcon("text-html");
            default:
                kWarning() << "Unknown project item type" << projectItem->type();
                break;
            }
            break;
        case Qt::ToolTipRole:
            switch ( projectItem->type() ) {
            case ProjectModelItem::OverviewItem:
                return i18nc("@info:tooltip", "An overview of the project %1.",
                             project->projectName());
            case ProjectModelItem::ScriptItem:
                return i18nc("@info:tooltip", "Create/edit the projects script.");
            case ProjectModelItem::AccessorItem:
                return i18nc("@info:tooltip", "Edit project settings directly in the XML "
                                              "source document. Intended for experts, normally "
                                              "the settings dialog should be used instead.");
            case ProjectModelItem::PlasmaPreviewItem:
                return i18nc("@info:tooltip", "Test the project in a PublicTransport applet in a "
                                              "Plasma preview");
            case ProjectModelItem::WebItem:
                if ( project->accessor()->info()->url().isEmpty() ) {
                    return i18nc("@info:tooltip", "Show the service providers home page.");
                } else {
                    return project->accessor()->info()->url();
                }
            default:
                kWarning() << "Unknown project item type" << projectItem->type();
                break;
            }
            break;
        case Qt::FontRole:
            switch ( projectItem->type() ) {
            case ProjectModelItem::AccessorItem:
                if ( projectItem->project()->projectSourceTab() &&
                     projectItem->project()->projectSourceTab()->isModified() )
                {
                    QFont font = KGlobalSettings::generalFont();
                    font.setItalic( true );
                    return font;
                } else {
                    return KGlobalSettings::generalFont();
                }
            case ProjectModelItem::ScriptItem:
                if ( projectItem->project()->scriptTab() &&
                     projectItem->project()->scriptTab()->isModified() )
                {
                    QFont font = KGlobalSettings::generalFont();
                    font.setItalic( true );
                    return font;
                } else {
                    return KGlobalSettings::generalFont();
                }
            case ProjectModelItem::PlasmaPreviewItem:
            case ProjectModelItem::WebItem:
            case ProjectModelItem::OverviewItem:
            default:
                return QVariant();
            }
        }
    } else {
        // Top level project item
        switch ( role ) {
        case Qt::DisplayRole: {
            return KDialog::makeStandardCaption( project->projectName(), 0, project->isModified()
                    ? KDialog::ModifiedCaption : KDialog::NoCaptionFlags );
        }
        case Qt::DecorationRole:
            return project->projectIcon();
        case Qt::ToolTipRole:
            return project->savePathInfoString();
        case Qt::FontRole:
            // Use italic font for modified projects and bold font for active projects
            if ( project->isModified() || project == m_activeProject ) {
                QFont font = KGlobalSettings::generalFont();
                font.setItalic( project->isModified() );
                font.setBold( project == m_activeProject );
                return font;
            } else {
                return KGlobalSettings::generalFont();
            }
        }
    }

    return QVariant();
}

Qt::ItemFlags ProjectModel::flags( const QModelIndex &index ) const
{
    if ( !index.isValid() ) {
        return Qt::NoItemFlags;
    }

    ProjectModelItem *projectItem = static_cast< ProjectModelItem* >( index.internalPointer() );
    Project *project = projectItem->project();

    if ( index.parent().isValid() ) {
        // Child item of a project item
        switch ( projectItem->type() ) {
        case ProjectModelItem::OverviewItem:
        case ProjectModelItem::AccessorItem:
        case ProjectModelItem::PlasmaPreviewItem:
        case ProjectModelItem::ScriptItem:
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        case ProjectModelItem::WebItem:
            if ( project->accessor()->info()->url().isEmpty() ) {
                // Disable web item if no home page URL has been specified
                return Qt::ItemIsSelectable;
            } else {
                return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
            }
        default:
            return Qt::NoItemFlags;
        }
    } else {
        // Top level project item
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }
}

ProjectModelItem::ProjectModelItem( Project *project, Type type, ProjectModelItem *parent )
        : m_project(project), m_parent(parent), m_type(type)
{
    if ( parent ) {
        parent->addChild( this );
    }
}

ProjectModelItem::~ProjectModelItem()
{
    qDeleteAll( m_children );

    if ( !m_parent ) {
        // Use deleteLater(), because otherwise using project action Project::Close causes a crash
        m_project->deleteLater();
    }
}

void ProjectModelItem::addChild( ProjectModelItem *item )
{
    if ( m_children.contains(item) ) {
        kDebug() << "Child item already added";
        return;
    }

    item->m_parent = this;
    m_children << item;
}

Project *ProjectModel::projectFromFilePath( const QString &filePath ) const
{
    foreach ( ProjectModelItem *projectItem, m_projects ) {
        if ( filePath == projectItem->project()->filePath() ) {
            return projectItem->project();
        }
    }

    return 0L;
}

void ProjectModel::appendProject( Project *project )
{
    // Create project root item
    beginInsertRows( QModelIndex(), m_projects.count(), m_projects.count() );
    ProjectModelItem *projectItem = new ProjectModelItem( project );
    m_projects << projectItem;
    endInsertRows();

    // Create child items
    const QModelIndex projectIndex = indexFromProjectItem( projectItem );
    beginInsertRows( projectIndex, 0, 4 );
    projectItem->addChild( ProjectModelItem::createOverviewtItem(project) );
    projectItem->addChild( ProjectModelItem::createScriptItem(project) );
    projectItem->addChild( ProjectModelItem::createAccessorDocumentItem(project) );
    projectItem->addChild( ProjectModelItem::createWebItem(project) );
    projectItem->addChild( ProjectModelItem::createPlasmaPreviewItem(project) );
    endInsertRows();

    project->setProjectModel( this );
    connect( project, SIGNAL(modifiedStateChanged(bool)), this, SLOT(slotProjectModified()) );
    connect( this, SIGNAL(activeProjectChanged(Project*,Project*)),
             project, SLOT(slotActiveProjectChanged(Project*,Project*)) );
    connect( project, SIGNAL(setAsActiveProjectRequest()), this, SLOT(setAsActiveProjectRequest()) );
    emit projectAdded( project );

    if ( !m_activeProject ) {
        // Make new project the active project if no other project is set
        setActiveProject( project );
    }
}

void ProjectModel::setAsActiveProjectRequest()
{
    Project *project = qobject_cast< Project* >( sender() );
    if ( project ) {
        setActiveProject( project );
    } else {
        kWarning() << "Slot setAsActiveProjectRequest() not called from Project";
    }
}

TabType ProjectModelItem::tabTypeFromProjectItemType( ProjectModelItem::Type projectItemType )
{
    switch ( projectItemType ) {
    case OverviewItem:
        return Tabs::Overview;
    case AccessorItem:
        return Tabs::ProjectSource;
    case ScriptItem:
        return Tabs::Script;
    case PlasmaPreviewItem:
        return Tabs::PlasmaPreview;
    case WebItem:
        return Tabs::Web;
    case ProjectItem:
    default:
        return Tabs::NoTab;
    }
}

ProjectModelItem::Type ProjectModelItem::projectItemTypeFromTabType( TabType tabType )
{
    switch ( tabType ) {
    case Tabs::Overview:
        return OverviewItem;
    case Tabs::ProjectSource:
        return AccessorItem;
    case Tabs::Script:
        return ScriptItem;
    case Tabs::PlasmaPreview:
        return PlasmaPreviewItem;
    case Tabs::Web:
        return WebItem;
    case Tabs::NoTab:
    default:
        return ProjectItem;
    }
}

void ProjectModel::slotProjectModified()
{
    Project *project = qobject_cast< Project* >( sender() );
    Q_ASSERT( project );

    // Inform about changes in the project root item
    const QModelIndex index = indexFromProject( project );
    emit dataChanged( index, index );

    // Inform about changes in the children of the project item
    ProjectModelItem *projectItem = projectItemFromIndex( index );
    if ( !projectItem->children().isEmpty() ) {
        const QModelIndex beginChild = createIndex( 0, 0,  projectItem->children().first() );
        const QModelIndex endChild = createIndex( m_projects.count() - 1, 0,
                                                  projectItem->children().last() );
        emit dataChanged( beginChild, endChild );
    }

    // Notify about changes in the project
    emit projectModified( project );
}

void ProjectModel::removeProject( Project *project )
{
    const QModelIndex index = indexFromProject( project );
    if ( !index.isValid() ) {
        kDebug() << "Project not found" << project->projectName();
        return;
    }

    removeRows( index.row(), 1 );
}

bool ProjectModel::removeRows( int row, int count, const QModelIndex &parent )
{
    beginRemoveRows( parent, row, row + count - 1 );
    for ( int i = 0; i < count; ++i ) {
        ProjectModelItem *projectItem = m_projects.takeAt( row );
        emit projectAboutToBeRemoved( projectItem->project() );

        if ( m_activeProject == projectItem->project() ) {
            // The active project gets removed, find another project to use as active project
            const int newRow = row <= 0 ? 1 : row - 1;
            if ( newRow >= rowCount() ) {
                // No other project found
                if ( m_activeProject ) {
                    setActiveProject( 0 );
                }
            } else {
                setActiveProject( m_projects[newRow]->project() );

                const QModelIndex index = indexFromRow( newRow );
                dataChanged( index, index );
            }
        }

        delete projectItem;
    }
    endRemoveRows();
    return true;
}

void ProjectModel::clear()
{
    foreach ( ProjectModelItem *projectItem, m_projects ) {
        emit projectAboutToBeRemoved( projectItem->project() );
    }

    setActiveProject( 0 );

    beginRemoveRows( QModelIndex(), 0, m_projects.count() );
    qDeleteAll( m_projects );
    m_projects.clear();
    endRemoveRows();

}

QModelIndex ProjectModel::indexFromRow( int row ) const
{
    if ( row < 0 || row >= m_projects.count() ) {
        return QModelIndex();
    } else {
        return createIndex( row, 0, m_projects[row] );
    }
}

QModelIndex ProjectModel::indexFromProject( Project *project ) const
{
    for ( int row = 0; row < m_projects.count(); ++row ) {
        ProjectModelItem *projectItem = m_projects[ row ];
        if ( projectItem->project() == project ) {
            return createIndex( row, 0, projectItem );
        }
    }

    // Project not found
    return QModelIndex();
}

QModelIndex ProjectModel::indexFromProjectItem( ProjectModelItem *projectItem ) const
{
    if ( projectItem->parent() ) {
        return createIndex( projectItem->parent()->children().indexOf(projectItem), 0, projectItem );
    } else {
        return createIndex( m_projects.indexOf(projectItem), 0, projectItem );
    }
}

ProjectModelItem *ProjectModel::projectItemFromIndex( const QModelIndex &index ) const
{
    return static_cast< ProjectModelItem* >( index.internalPointer() );
}

ProjectModelItem *ProjectModel::projectItemFromRow( int row ) const
{
    if ( row < 0 || row >= m_projects.count() ) {
        return 0;
    } else {
        return m_projects[ row ];
    }
}

ProjectModelItem *ProjectModel::projectItemChildFromProject( Project *project,
                                                             ProjectModelItem::Type type ) const
{
    ProjectModelItem *projectItem = projectItemFromProject( project );
    if ( !projectItem ) {
        return 0;
    }

    foreach ( ProjectModelItem *item, projectItem->children() ) {
        if ( item->type() == type ) {
            return item;
        }
    }

    // Not found
    return 0;
}
