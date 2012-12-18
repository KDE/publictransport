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
#include "testdockwidget.h"

// Own includes
#include "../projectmodel.h"
#include "../project.h"
#include "../testmodel.h"
#include "../tabs/webtab.h"

// KDE includes
#include <KLocalizedString>
#include <KWebView>

// Qt includes
#include <QTreeView>
#include <QFormLayout>
#include <QHeaderView>
#include <QMenu>
#include <QApplication>
#include <QClipboard>

TestDockWidget::TestDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                                QWidget *parent )
        : AbstractDockWidget( i18nc("@window:title Dock title", "Test"), showDocksAction, parent ),
          m_projectModel(projectModel), m_testModel(0), m_testWidget(new QTreeView(this))
{
    setObjectName( "test" );

    setWhatsThis( i18nc("@info:whatsthis", "<title>Shows test results</title>"
                        "<para>Test results get updated dynamically while the tests are running. "
                        "Check child items for additional information about warnings or errors."
                        "</para>") );

    m_testWidget->setAnimated( true );
    m_testWidget->setAllColumnsShowFocus( true );
    m_testWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    m_testWidget->setSelectionMode( QAbstractItemView::SingleSelection );
    QSizePolicy sizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    sizePolicy.setVerticalStretch( 1 );
    m_testWidget->setSizePolicy( sizePolicy );
    m_testWidget->setWordWrap( true );
    m_testWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    connect( m_testWidget, SIGNAL(customContextMenuRequested(QPoint)),
             this, SLOT(contextMenu(QPoint)) );
    connect( m_testWidget, SIGNAL(clicked(QModelIndex)), this, SLOT(itemClicked(QModelIndex)) );

    connect( projectModel, SIGNAL(activeProjectAboutToChange(Project*,Project*)),
             this, SLOT(activeProjectAboutToChange(Project*,Project*)) );
    if ( projectModel->activeProject() ) {
        initModel( projectModel->activeProject()->testModel() );
    }

    QWidget *widget = new QWidget( this );
    widget->setMinimumSize( 200, 100 );
    QFormLayout *testLayout = new QFormLayout( widget );
    testLayout->setContentsMargins( 0, 0, 0, 0 );
    testLayout->setVerticalSpacing( 0 );
    testLayout->setRowWrapPolicy( QFormLayout::WrapLongRows );
    testLayout->addRow( m_testWidget );
    setWidget( widget );
}

TestDockWidget::~TestDockWidget()
{
}

void TestDockWidget::activeProjectAboutToChange( Project *project, Project *previousProject )
{
    Q_UNUSED( previousProject );
    if ( m_testModel ) {
        disconnect( m_testModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
                    this, SLOT(rowsInserted(QModelIndex,int,int)) );
    }

    if ( project ) {
        initModel( project->testModel() );
    } else {
        m_testModel = new TestModel( this );
        m_testWidget->setModel( m_testModel );
    }
}

void TestDockWidget::initModel( TestModel *testModel )
{
    m_testWidget->setModel( testModel );
    if ( m_testModel && m_testModel->QObject::parent() == this ) {
        delete m_testModel;
    }
    m_testModel = testModel;

    // Initialize header
    QHeaderView *header = m_testWidget->header();
    header->setDefaultSectionSize( 150 );
    header->setResizeMode( static_cast<int>(TestModel::NameColumn), QHeaderView::Interactive );
    header->setResizeMode( static_cast<int>(TestModel::StateColumn), QHeaderView::ResizeToContents );
    header->setResizeMode( static_cast<int>(TestModel::ExplanationColumn), QHeaderView::Stretch );

    connect( m_testModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
             this, SLOT(rowsInserted(QModelIndex,int,int)) );

    // Expand test case items
    m_testWidget->reset();
    for ( int testCase = 0; testCase < m_testModel->rowCount(); ++testCase ) {
        const QModelIndex testCaseIndex = m_testModel->index( testCase, 0 );
        if ( !m_testWidget->isExpanded(testCaseIndex) ) {
            m_testWidget->expand( testCaseIndex );
        }

        // Make the first column of test item children spanned
        for ( int test = 0; test < m_testModel->rowCount(testCaseIndex); ++test ) {
            const QModelIndex testIndex = m_testModel->index( test, 0, testCaseIndex );
            for ( int child = 0; child < m_testModel->rowCount(testIndex); ++child ) {
                m_testWidget->setFirstColumnSpanned( child, testIndex, true );
            }
        }
    }
}

void TestDockWidget::rowsInserted( const QModelIndex &parent, int first, int last )
{
    // Make the first column of test item children spanned
    if ( parent.isValid() && parent.parent().isValid() ) {
        for ( int row = first; row <= last; ++row ) {
            m_testWidget->setFirstColumnSpanned( row, parent, true );
        }
    }
}

void TestDockWidget::itemClicked( const QModelIndex &index )
{
    if ( index.parent().isValid() && index.parent().parent().isValid() &&
         index.data(TestModel::LineNumberRole).isValid() )
    {
        const int lineNumber = index.data( TestModel::LineNumberRole ).toInt();
        const QString fileName = index.data( TestModel::FileNameRole ).toString();
        emit clickedTestErrorItem( fileName, lineNumber, index.data().toString() );
    }
}

void TestDockWidget::contextMenu( const QPoint &pos )
{
    const QModelIndex index = m_testWidget->indexAt( pos );
    if ( !m_testModel || !m_projectModel->activeProject() ) {
        return;
    }

    QPointer< QMenu > menu( new QMenu(this) );
    QAction *openUrlAction = 0, *copyUrlAction = 0;
    TestModel::Test test = TestModel::InvalidTest;
    TestModel::TestCase testCase = TestModel::InvalidTestCase;

    if ( index.isValid() ) {
        test = m_testModel->testFromIndex( index );
        testCase = m_testModel->testCaseFromIndex( index );
        if ( test != TestModel::InvalidTest ) {
            menu->addAction( m_projectModel->activeProject()->projectAction(
                    Project::RunSpecificTest, QVariant::fromValue(static_cast<int>(test))) );
        } else if ( testCase != TestModel::InvalidTestCase ) {
            menu->addAction( m_projectModel->activeProject()->projectAction(
                    Project::RunSpecificTestCase, QVariant::fromValue(static_cast<int>(testCase))) );
        }

        TimetableDataRequestMessage::Features features =
                static_cast<TimetableDataRequestMessage::Features>(
                index.data(TestModel::FeatureRole).toInt() );
        if ( features.testFlag(TimetableDataRequestMessage::OpenLink) ) {
            openUrlAction = menu->addAction( KIcon("document-open-remote"),
                                             i18nc("@info/plain", "Open URL") );
            copyUrlAction = menu->addAction( KIcon("edit-copy"), i18nc("@info/plain", "Copy URL") );
        }

        // Show solution actions in the context menu
        QAction *solutionAction = TestModel::actionFromIndex( index );
        if ( solutionAction ) {
            menu->addAction( solutionAction );
        }
    }

    menu->addSeparator();
    menu->addAction( m_projectModel->activeProject()->projectAction(Project::ClearTestResults) );

    QAction *action = menu->exec( m_testWidget->mapToGlobal(pos) );
    if ( action ) {
        if ( action == openUrlAction ) {
            const QString url = index.data( TestModel::UrlRole ).toString();
            m_projectModel->activeProject()->showWebTab();
            m_projectModel->activeProject()->webTab()->webView()->load( url );
        } else if ( action == copyUrlAction ) {
            const QString url = index.data( TestModel::UrlRole ).toString();
            QApplication::clipboard()->setText( url );
        }
    }
    delete menu.data();
}

QWidget *TestDockWidget::mainWidget() const {
    return m_testWidget;
}
