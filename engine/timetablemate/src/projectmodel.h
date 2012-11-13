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

#ifndef PROJECTMODEL_H
#define PROJECTMODEL_H

// Own includes
#include "config.h"
#include "tabs/tabs.h"
#include "javascriptparser.h"
#include "testmodel.h" // For TestModel::Test

// Qt includes
#include <QAbstractItemModel>

namespace ThreadWeaver {
    class WeaverInterface;
}
typedef QSharedPointer< ThreadWeaver::WeaverInterface > WeaverInterfacePointer;

class Project;
class ProjectModel;
class QTimer;

/** @brief An item of ProjectModel. */
class ProjectModelItem {
    friend class ProjectModel;

public:
    /** @brief The type of the item. */
    enum Type {
        ProjectItem = 0, /**< A top level project item. */

        DashboardItem, /**< A child of the project item, shows the dashboard of the project. */
        ProjectSourceItem, /**< A child of the project item, represents the project source XML
          document, ie the service provider plugin XML document. */
        ScriptItem, /**< A child of the project item, represents the script document. */
        IncludedScriptItem, /**< A child of the project item, represents an included script document. */
        CodeItem, /**< A child of the script item, represents a code node. */
        GtfsDatabaseItem, /**< A child of the project item, shows the GTFS database of the provider. */
        WebItem, /**< A child of the project item, represents the web view. */
        PlasmaPreviewItem /**< A child of the project item, represents the plasma preview. */
    };

    virtual ~ProjectModelItem();

    virtual QString text() const;

    Project *project() const { return m_project; };
    ProjectModelItem *parent() const { return m_parent; };
    QList< ProjectModelItem* > children() const { return m_children; };
    ProjectModelItem *firstChildFromType( Type type ) const;
    QList<ProjectModelItem*> childrenFromType( Type type ) const;
    Type type() const { return m_type; };
    static TabType tabTypeFromProjectItemType( Type projectItemType );
    static Type projectItemTypeFromTabType( TabType tabType );

    inline bool isProjectItem() const { return m_type == ProjectItem; };
    inline bool isProjectSourceItem() const { return m_type == ProjectSourceItem; };
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    inline bool isScriptItem() const { return m_type == ScriptItem; };
    inline bool isIncludedScriptItem() const { return m_type == IncludedScriptItem; };
#else
    inline bool isScriptItem() const { return false; };
    inline bool isIncludedScriptItem() const { return false; };
#endif
#ifdef BUILD_PROVIDER_TYPE_GTFS
    inline bool isGtfsDatabaseItem() const { return m_type == GtfsDatabaseItem; };
#else
    inline bool isGtfsDatabaseItem() const { return false; };
#endif
    inline bool isPlasmaPreviewItem() const { return m_type == PlasmaPreviewItem; };
    inline bool isWebItem() const { return m_type == WebItem; };

protected:
    ProjectModelItem( Project *project, Type type = ProjectItem, ProjectModelItem *parent = 0 );

    static ProjectModelItem *createDashboardtItem( Project *project ) {
            return new ProjectModelItem( project, DashboardItem ); };
    static ProjectModelItem *createProjectSourceDocumentItem( Project *project ) {
            return new ProjectModelItem( project, ProjectSourceItem ); };
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    static ProjectModelItem *createScriptItem( Project *project ) {
            return new ProjectModelItem( project, ScriptItem ); };
#endif
#ifdef BUILD_PROVIDER_TYPE_GTFS
    static ProjectModelItem *createGtfsDatabaseItem( Project *project ) {
            return new ProjectModelItem( project, GtfsDatabaseItem ); };
#endif
    static ProjectModelItem *createPlasmaPreviewItem( Project *project ) {
            return new ProjectModelItem( project, PlasmaPreviewItem ); };
    static ProjectModelItem *createWebItem( Project *project ) {
            return new ProjectModelItem( project, WebItem ); };

    void addChild( ProjectModelItem *item );
    void insertChild( int index, ProjectModelItem *item );
    void removeChildren( const QList< ProjectModelItem* > &items );
    void clearChildren();

private:
    Project *m_project;
    ProjectModelItem *m_parent;
    QList< ProjectModelItem* > m_children;
    Type m_type;
};

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
/**
 * @brief An item of ProjectModel, which represents child items of script items.
 **/
class ProjectModelCodeItem : public ProjectModelItem {
    friend class ProjectModel;

public:
    virtual QString text() const { return m_node->text(); };
    CodeNode::Ptr node() const { return m_node; };

protected:
    ProjectModelCodeItem( Project *project, const CodeNode::Ptr &node,
                          ProjectModelItem *parent = 0 )
            : ProjectModelItem(project, CodeItem, parent), m_node(node) {};

private:
    CodeNode::Ptr m_node;
};

/**
 * @brief An item of ProjectModel, which represents an included script document.
 **/
class ProjectModelIncludedScriptItem : public ProjectModelItem {
    friend class ProjectModel;

public:
    virtual QString text() const { return fileName(); };
    QString fileName() const;
    QString filePath() const { return m_filePath; };

protected:
    ProjectModelIncludedScriptItem( Project *project, const QString &filePath,
                                    ProjectModelItem *parent = 0 )
            : ProjectModelItem(project, IncludedScriptItem, parent), m_filePath(filePath) {};

private:
    QString m_filePath;
};
#endif // BUILD_PROVIDER_TYPE_SCRIPT

/**
 * @brief A model for TimetableMate projects.
 *
 * One project is always the active one if at least one project is opened. Get the currently active
 * project with activeProject() and connect to the activeProjectAboutToChange() signal to get
 * notified when the active project changes.
 **/
class ProjectModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_PROPERTY( Project* activeProject READ activeProject WRITE setActiveProject NOTIFY activeProjectAboutToChange )
    friend class ProjectModelItem;
    friend class Project;

public:
    ProjectModel( QObject *parent = 0 );
    ~ProjectModel();

    virtual int columnCount( const QModelIndex &parent = QModelIndex() ) const {
        Q_UNUSED( parent );
        return 1;
    };
    virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual QModelIndex parent( const QModelIndex &child ) const;
    virtual QModelIndex index( int row, int column,
                               const QModelIndex &parent = QModelIndex() ) const;
    virtual QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const;
    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;
    virtual bool removeRows( int row, int count, const QModelIndex &parent = QModelIndex() );
    void clear();

    QModelIndex indexFromRow( int row ) const;
    QModelIndex indexFromProject( Project *project ) const;
    QModelIndex indexFromProjectItem( ProjectModelItem *projectItem ) const;

    ProjectModelItem *projectItemFromIndex( const QModelIndex &index ) const;
    ProjectModelItem *projectItemFromRow( int row ) const;
    inline ProjectModelItem *projectItemFromProject( Project *project ) const {
            return projectItemFromIndex(indexFromProject(project)); };
    ProjectModelItem *projectItemChildFromProject( Project *project,
                                                   ProjectModelItem::Type type ) const;

    /** @brief Get a list of all ProjectModelItem objects in this model. */
    QList< ProjectModelItem* > projectItems() const { return m_projects; };

    /** @brief Get the currently active project. */
    Project *activeProject() const { return m_activeProject; };

    /** @brief Get the project that is stored at @p filePath, if any. */
    Project *projectFromFilePath( const QString &filePath ) const;

    /** @brief Whether or not the project stored at @p projectFilePath is already loaded. */
    inline bool isProjectLoaded( const QString &projectFilePath ) const {
            return projectFromFilePath(projectFilePath) != 0; };

    bool isIdle() const;
    WeaverInterfacePointer weaver() const { return m_weaver; };

signals:
    /** @brief Emitted when @p project gets added to the model. */
    void projectAdded( Project *project );

    /** @brief Emitted before @p project gets removed from the model. */
    void projectAboutToBeRemoved( Project *project );

    /** @brief Emitted when @p project gets modified. */
    void projectModified( Project *project );

    /** @brief Emitted before the active project changes from @p previousProject to @p project. */
    void activeProjectAboutToChange( Project *project, Project *previousProject );

    /** @brief Emitted when the active project changed from @p previousProject to @p project. */
    void activeProjectChanged( Project *project, Project *previousProject );

    void testProgress( int finishedTestCount, int totalTestCount );
    void testProgress( const QHash< Project*, QList< TestModel::Test > > &finishedTests,
                       const QHash< Project*, QList< TestModel::Test > > &allTests );

    void idleChanged( bool isIdle );

public slots:
    /** @brief Append @p project to the list of projects in this model. */
    void appendProject( Project *project );

    /** @brief Remove @p project from this model. */
    void removeProject( Project *project );

    /** @brief Set the currently active project. */
    void setActiveProject( Project *activeProject );

    /** @brief Set the active project to the project at @p row in this model. */
    inline void setActiveProjectFromRow( int row ) {
        ProjectModelItem *projectItem = projectItemFromRow( row );
        setActiveProject( projectItem ? projectItem->project() : 0 );
    };
    void scriptSaved();

    void testAllProjects();

protected slots:
    void updateIsIdle();
    void slotProjectModified();
    void setAsActiveProjectRequest();
    void projectTestProgress( const QList< TestModel::Test > &finishedTests,
                              const QList< TestModel::Test > &startedTests );

    /** @brief The type of a provider changed, signaled from Project. */
    void providerTypeChanged( Enums::ServiceProviderType newType,
                              Enums::ServiceProviderType oldType );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    void updateProjects();
#endif

private:
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    void insertCodeNodes( ProjectModelItem *scriptItem, bool emitSignals = true );
#endif

    QList< ProjectModelItem* > m_projects;
    Project *m_activeProject; // Only to be changed using setActiveProject()
    QTimer *m_updateProjectsTimer;
    QList< Project* > m_changedScriptProjects;
    QHash< Project*, QList< TestModel::Test > > m_startedTests;
    QHash< Project*, QList< TestModel::Test > > m_finishedTests;
    WeaverInterfacePointer m_weaver;
    bool m_idle;
};

#endif // Multiple inclusion guard
