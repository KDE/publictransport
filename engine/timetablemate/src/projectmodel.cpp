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
#include "tabs/projectsourcetab.h"
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    #include "tabs/scripttab.h"
    #include "debugger/debugger.h"
    #include "javascriptparser.h"
    #include "javascriptmodel.h"
#endif

// Public Transport engine includes
#include <engine/serviceprovider.h>
#include <engine/serviceproviderdata.h>

// KDE includes
#include <KLocalizedString>
#include <KIcon>
#include <KGlobalSettings>
#include <KDebug>
#include <KDialog>
#include <KColorScheme>
#include <ThreadWeaver/Weaver>

// Qt includes
#include <QFileInfo>
#include <QTimer>
#include <QFont>

ProjectModel::ProjectModel( QObject *parent )
        : QAbstractItemModel(parent), m_activeProject(0), m_updateProjectsTimer(0),
          m_weaver(WeaverInterfacePointer(new ThreadWeaver::Weaver()))
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
    case ProjectModelItem::DashboardItem:
        return i18nc("@info/plain", "Dashboard");
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case ProjectModelItem::ScriptItem: {
        QString name = m_project->scriptFileName().isEmpty()
                ? i18nc("@info/plain", "Script File")
                : QFileInfo(m_project->scriptFileName()).fileName();
        return KDialog::makeStandardCaption( name, 0,
                m_project->scriptTab() && m_project->scriptTab()->isModified()
                ? KDialog::ModifiedCaption : KDialog::NoCaptionFlags );
    }
#endif
    case ProjectModelItem::ProjectSourceItem: {
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
            case ProjectModelItem::DashboardItem:
                return KIcon("dashboard-show");
            case ProjectModelItem::ProjectSourceItem:
                return KIcon("application-x-publictransport-serviceprovider");
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
            case ProjectModelItem::ScriptItem:
            case ProjectModelItem::IncludedScriptItem:
                return project->scriptIcon();
            case ProjectModelItem::CodeItem:
                return KIcon("code-function");
#endif
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
            case ProjectModelItem::DashboardItem:
                return i18nc("@info:tooltip", "The dashboard of the project %1.",
                             project->projectName());
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
            case ProjectModelItem::ScriptItem:
                return i18nc("@info:tooltip", "Create/edit the projects script.");
            case ProjectModelItem::IncludedScriptItem:
                return i18nc("@info:tooltip", "View/edit included script <filename>%1</filename>.",
                             dynamic_cast<ProjectModelIncludedScriptItem*>(projectItem)->fileName() );
#endif
            case ProjectModelItem::ProjectSourceItem:
                return i18nc("@info:tooltip", "Edit project settings directly in the XML "
                                              "source document. Intended for experts, normally "
                                              "the settings dialog should be used instead.");
            case ProjectModelItem::PlasmaPreviewItem:
                return i18nc("@info:tooltip", "Test the project in a PublicTransport applet in a "
                                              "Plasma preview");
            case ProjectModelItem::WebItem:
                if ( project->provider()->data()->url().isEmpty() ) {
                    return i18nc("@info:tooltip", "Show the service providers home page.");
                } else {
                    return project->provider()->data()->url();
                }
            default:
                kWarning() << "Unknown project item type" << projectItem->type();
                break;
            }
            break;
        case Qt::FontRole:
            switch ( projectItem->type() ) {
            case ProjectModelItem::ProjectSourceItem:
                if ( project->projectSourceTab() &&
                     project->projectSourceTab()->isModified() )
                {
                    QFont font = KGlobalSettings::generalFont();
                    font.setItalic( true );
                    return font;
                }
                break;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
            case ProjectModelItem::ScriptItem:
                if ( project->scriptTab() &&
                     project->scriptTab()->isModified() )
                {
                    QFont font = KGlobalSettings::generalFont();
                    font.setItalic( true );
                    return font;
                }
                break;
            case ProjectModelItem::IncludedScriptItem: {
                ProjectModelIncludedScriptItem *includedScriptItem =
                        dynamic_cast< ProjectModelIncludedScriptItem* >( projectItem );
                ScriptTab *scriptTab = project->scriptTab( includedScriptItem->filePath() );
                if ( scriptTab && scriptTab->isModified() ) {
                    QFont font = KGlobalSettings::generalFont();
                    font.setItalic( true );
                    return font;
                }
            } break;
#endif
            case ProjectModelItem::PlasmaPreviewItem:
            case ProjectModelItem::WebItem:
            case ProjectModelItem::DashboardItem:
            case ProjectModelItem::CodeItem:
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
            }
            break;
        case Qt::ForegroundRole:
            switch ( project->testModel()->completeState() ) {
            case TestModel::TestFinishedSuccessfully:
                return KColorScheme(QPalette::Active).foreground(KColorScheme::PositiveText);
            case TestModel::TestFinishedWithWarnings:
                return KColorScheme(QPalette::Active).foreground(KColorScheme::NeutralText);
            case TestModel::TestFinishedWithErrors:
            case TestModel::TestCouldNotBeStarted:
                return KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText);
            case TestModel::TestNotStarted:
            case TestModel::TestIsRunning:
            default:
                return QVariant();
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
        case ProjectModelItem::DashboardItem:
        case ProjectModelItem::ProjectSourceItem:
        case ProjectModelItem::PlasmaPreviewItem:
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        case ProjectModelItem::ScriptItem:
        case ProjectModelItem::IncludedScriptItem:
        case ProjectModelItem::CodeItem:
#endif
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        case ProjectModelItem::WebItem:
            if ( project->provider()->data()->url().isEmpty() ) {
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
        kDebug() << "Child item already added" << item->text();
        return;
    }

    item->m_parent = this;
    m_children << item;
}

void ProjectModelItem::insertChild( int index, ProjectModelItem *item )
{
    if ( m_children.contains(item) ) {
        kDebug() << "Child item already added" << item->text();
        return;
    }

    item->m_parent = this;
    m_children.insert( index, item );
}

void ProjectModelItem::removeChildren( const QList< ProjectModelItem* > &items )
{
    foreach ( ProjectModelItem *item, items ) {
        m_children.removeOne( item );
        delete item;
    }
}

void ProjectModelItem::clearChildren()
{
    qDeleteAll( m_children );
    m_children.clear();
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
    projectItem->addChild( ProjectModelItem::createDashboardtItem(project) );
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    projectItem->addChild( ProjectModelItem::createScriptItem(project) );
#endif
    projectItem->addChild( ProjectModelItem::createProjectSourceDocumentItem(project) );
    projectItem->addChild( ProjectModelItem::createWebItem(project) );
    projectItem->addChild( ProjectModelItem::createPlasmaPreviewItem(project) );
    endInsertRows();

    connect( project, SIGNAL(modifiedStateChanged(bool)), this, SLOT(slotProjectModified()) );
    connect( project, SIGNAL(debuggerReady()), this, SLOT(scriptSaved()) );
    connect( project->testModel(), SIGNAL(testResultsChanged()), this, SLOT(slotProjectModified()) );
    connect( this, SIGNAL(activeProjectChanged(Project*,Project*)),
             project, SLOT(slotActiveProjectChanged(Project*,Project*)) );
    connect( project, SIGNAL(setAsActiveProjectRequest()), this, SLOT(setAsActiveProjectRequest()) );
    connect( project, SIGNAL(testProgress(QList<TestModel::Test>,QList<TestModel::Test>)),
             this, SLOT(projectTestProgress(QList<TestModel::Test>,QList<TestModel::Test>)) );
    project->setProjectModel( this );
    emit projectAdded( project );

    if ( !m_activeProject ) {
        // Make new project the active project if no other project is set
        setActiveProject( project );
    }
}

bool ProjectModel::isIdle() const
{
    foreach ( ProjectModelItem *projectItem, m_projects ) {
        Project *project = projectItem->project();
        if ( project->isTestRunning() ) {
            return false;
        }
    }

    return true;
}

void ProjectModel::updateProjects()
{
    if ( !isIdle() ) {
        // Too busy, try again later
        m_updateProjectsTimer->start( 500 );
        return;
    }

    delete m_updateProjectsTimer;
    m_updateProjectsTimer = 0;

    foreach ( Project *project, m_changedScriptProjects ) {
        // Remove all items for previously included files
        QList< ProjectModelItem* > itemsToRemove;
        const QModelIndex projectIndex = indexFromProject( project );
        ProjectModelItem *projectItem = projectItemFromIndex( projectIndex );
        ProjectModelItem *scriptItem = projectItemChildFromProject( project, ProjectModelItem::ScriptItem );
        int scriptItemIndex = indexFromProjectItem( scriptItem ).row();
        int firstIncludedScriptItemIndex = scriptItemIndex + 1;
        foreach ( ProjectModelItem *childItem, projectItem->children() ) {
            if ( childItem->type() == ProjectModelItem::IncludedScriptItem ) {
                itemsToRemove << childItem;
            }
        }
        if ( !itemsToRemove.isEmpty() ) {
            beginRemoveRows( projectIndex, firstIncludedScriptItemIndex,
                            firstIncludedScriptItemIndex + itemsToRemove.count() - 1 );
            projectItem->removeChildren( itemsToRemove );
            itemsToRemove.clear();
            endRemoveRows();
        }

        // Insert items for now included files
        if ( !project->includedFiles().isEmpty() ) {
            beginInsertRows( projectIndex, scriptItemIndex + 1,
                            scriptItemIndex + project->includedFiles().count() );
            foreach ( const QString &includedFile, project->includedFiles() ) {
                ProjectModelIncludedScriptItem *includedScriptItem =
                        new ProjectModelIncludedScriptItem( project, includedFile );
                projectItem->insertChild( ++scriptItemIndex, includedScriptItem );
                insertCodeNodes( includedScriptItem, false );
            }
            endInsertRows();
        }

        insertCodeNodes( scriptItem );
    }

    m_changedScriptProjects.clear();
}

void ProjectModel::scriptSaved()
{
    Project *project = qobject_cast< Project* >( sender() );
    Q_ASSERT( project );
    if ( !m_changedScriptProjects.contains(project) ) {
        m_changedScriptProjects << project;
    }

    if ( project->suppressMessages() ) {
        return;
    }
    if ( !m_updateProjectsTimer ) {
        m_updateProjectsTimer = new QTimer( this );
        connect( m_updateProjectsTimer, SIGNAL(timeout()), this, SLOT(updateProjects()) );
    }
    m_updateProjectsTimer->start( 250 );
}

void ProjectModel::insertCodeNodes( ProjectModelItem *scriptItem, bool emitSignals )
{
    // Parse script
    Project *project = scriptItem->project();
    ProjectModelIncludedScriptItem *includedScriptItem =
            dynamic_cast< ProjectModelIncludedScriptItem* >( scriptItem );
    JavaScriptParser parser( project->scriptText(
            includedScriptItem ? includedScriptItem->filePath() : QString()) );
    const QList< CodeNode::Ptr > nodes = parser.nodes();
    QList< CodeNode::Ptr > flatNodes = QList< CodeNode::Ptr >() << nodes;
    foreach ( const CodeNode::Ptr &node, nodes ) {
        flatNodes << JavaScriptModel::childFunctions( node );
    }

    // Remove old script child items
    const QModelIndex index = indexFromProjectItem( scriptItem );
    if ( !scriptItem->children().isEmpty() ) {
        if ( emitSignals ) {
            beginRemoveRows( index, 0, rowCount(index) );
            scriptItem->clearChildren();
            endRemoveRows();
        } else {
            scriptItem->clearChildren();
        }
    }

    // Insert new script child items
    if ( emitSignals ) {
        beginInsertRows( index, 0, flatNodes.count() - 1 );
    }
    foreach ( const CodeNode::Ptr &node, flatNodes ) {
        const FunctionNode::Ptr functionNode = node.dynamicCast<FunctionNode>();
        if ( functionNode ) {
            scriptItem->addChild( new ProjectModelCodeItem(project, functionNode) );
        }
    }
    if ( emitSignals ) {
        endInsertRows();
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
    case DashboardItem:
        return Tabs::Dashboard;
    case ProjectSourceItem:
        return Tabs::ProjectSource;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case ScriptItem:
    case IncludedScriptItem:
        return Tabs::Script;
#endif
    case PlasmaPreviewItem:
        return Tabs::PlasmaPreview;
    case WebItem:
        return Tabs::Web;
    case CodeItem:
    case ProjectItem:
    default:
        return Tabs::NoTab;
    }
}

ProjectModelItem::Type ProjectModelItem::projectItemTypeFromTabType( TabType tabType )
{
    switch ( tabType ) {
    case Tabs::Dashboard:
        return DashboardItem;
    case Tabs::ProjectSource:
        return ProjectSourceItem;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case Tabs::Script:
        return ScriptItem;
#endif
    case Tabs::PlasmaPreview:
        return PlasmaPreviewItem;
    case Tabs::Web:
        return WebItem;
    case Tabs::NoTab:
    default:
        return ProjectItem;
    }
}

QString ProjectModelIncludedScriptItem::fileName() const
{
    return QFileInfo( m_filePath ).fileName();
}

void ProjectModel::slotProjectModified()
{
    Project *project = qobject_cast< Project* >( sender() );
    if ( !project ) {
        TestModel *testModel = qobject_cast< TestModel* >( sender() );
        Q_ASSERT( testModel );

        project = testModel->project();
    }
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

void ProjectModel::testAllProjects()
{
    // First store all tests of all projects in the list of started tests,
    // otherwise the total number of tests isn't ready when testing starts
    foreach ( ProjectModelItem *projectItem, m_projects ) {
        m_startedTests.insert( projectItem->project(), TestModel::allTests() );
    }

    // Start the tests
    foreach ( ProjectModelItem *projectItem, m_projects ) {
        projectItem->project()->testProject();
    }
}

void ProjectModel::projectTestProgress( const QList< TestModel::Test > &projectFinishedTests,
                                        const QList< TestModel::Test > &projectStartedTests )
{
    Project *progressProject = qobject_cast< Project* >( sender() );
    Q_ASSERT( progressProject );

    QHash< Project*, QList< TestModel::Test > > finishedTests = m_finishedTests;
    QHash< Project*, QList< TestModel::Test > > startedTests;
    int finishedTestCount = 0, startedTestCount = 0;
    foreach ( ProjectModelItem *projectItem, m_projects ) {
        Project *project = projectItem->project();
        const QList< TestModel::Test > finishedTestsOfProject =
                project == progressProject ? projectFinishedTests : project->finishedTests();
        const QList< TestModel::Test > startedTestsOfProject =
                project == progressProject ? projectStartedTests : project->startedTests();

        if ( finishedTestsOfProject.isEmpty() ) {
            // Finished tests are not available from Project
            // after the test was finished for the project, add number of finished tests
            // stored in m_finishedTests
            finishedTestCount += m_finishedTests[ project ].count();
        } else {
            finishedTests.insert( project, finishedTestsOfProject );
            finishedTestCount += finishedTestsOfProject.count();
        }
        if ( !startedTestsOfProject.isEmpty() ) {
            startedTests.insert( project, startedTestsOfProject );
            startedTestCount += startedTestsOfProject.count();
        }
    }

    // Store finished tests and use stored list of started tests, if any
    m_finishedTests = finishedTests;
    if ( m_startedTests.isEmpty() ) {
        m_startedTests = startedTests;
    } else {
        startedTests = m_startedTests;

        startedTestCount = 0;
        for ( QHash< Project*, QList< TestModel::Test > >::ConstIterator it = startedTests.constBegin();
              it != startedTests.constEnd(); ++it )
        {
            startedTestCount += it->count();
        }
    }

    emit testProgress( finishedTestCount, startedTestCount );
    emit testProgress( finishedTests, startedTests );

    if ( finishedTestCount == startedTestCount ) {
        m_startedTests.clear();
        m_finishedTests.clear();
    }
}
