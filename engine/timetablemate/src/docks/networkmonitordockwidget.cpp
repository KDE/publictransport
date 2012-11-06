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
#include "networkmonitordockwidget.h"

// Own includes
#include "../projectmodel.h"
#include "../project.h"
#include "../networkmonitormodel.h"
#include "../tabs/webtab.h"

// KDE includes
#include <KLocalizedString>
#include <KActionMenu>

// Qt includes
#include <QWebInspector>
#include <QLabel>
#include <QTreeView>
#include <QHeaderView>
#include <QMenu>
#include <QApplication>
#include <QClipboard>

NetworkMonitorDockWidget::NetworkMonitorDockWidget( ProjectModel *projectModel,
                                                    KActionMenu *showDocksAction, QWidget *parent )
        : AbstractDockWidget( i18nc("@window:title Dock title", "Network Monitor"),
                             showDocksAction, parent ),
          m_widget(new QTreeView(this)), m_filterModel(new NetworkMonitorFilterModel(this))
{
    setObjectName( "networkmonitor" );

    setWhatsThis( i18nc("@info:whatsthis",
            "<title>Network Monitor</title>"
            "<para>Shows requests created in the web tab of the active project. Can be useful "
            "when trying to find out how to request a document containing timetable data.</para>"
            "<para>You can copy the URL or posted/received data using the context menu. Request "
            "URLs and posted data can be used as a template, into which the script can insert "
            "values like the stop name. Reply data can be used when writing the parsing code.</para>"
            "<para>If a document was requested using POST, you should use "
            "<icode>network.post(request)</icode>, otherwise use "
            "<icode>network.get(request)</icode> in your script. Or use the synchronous variants "
            "(see the <interface>Documentation</interface> dock for more inforamtion about the "
            "<icode>network</icode> script object).</para>"
            "<para>For a more detailed analysis of network requests and replies you can use the "
            "<interface>Web Inspector</interface> dock or a tool like "
            "<emphasis>wireshark</emphasis>.</para>") );

    m_filterModel->setDynamicSortFilter( true );
    m_widget->setModel( m_filterModel );
    m_widget->setAlternatingRowColors( true );
    m_widget->setTextElideMode( Qt::ElideMiddle );
    m_widget->setContextMenuPolicy( Qt::CustomContextMenu );
    m_widget->setMinimumSize( 150, 100 );
    connect( m_widget, SIGNAL(customContextMenuRequested(QPoint)),
             this, SLOT(contextMenu(QPoint)) );
    setWidget( m_widget );

    connect( projectModel, SIGNAL(activeProjectAboutToChange(Project*,Project*)),
             this, SLOT(activeProjectAboutToChange(Project*,Project*)) );
}

void NetworkMonitorDockWidget::contextMenu( const QPoint &pos )
{
    const QModelIndex index = m_widget->indexAt( pos );
    QList< QAction* > actions;
    QPointer<QMenu> menu( new QMenu(this) );

    QAction *copyAction = 0;
    if ( index.isValid() && (index.column() == NetworkMonitorModel::UrlColumn ||
                             index.column() == NetworkMonitorModel::DataColumn) )
    {
        copyAction = new QAction( KIcon("edit-copy"), i18nc("@info/plain", "Copy"), menu );
        actions << copyAction;
    }
    QAction *clearAction = new QAction( KIcon("edit-clear-list"), i18nc("@info/plain", "Clear"), menu );
    actions << clearAction;

    QAction *separator = new QAction( menu );
    separator->setSeparator( true );
    QAction *showHtmlData = new QAction( i18nc("@ịnfo:action", "Show HTML Requests/Replies"), menu );
    QAction *showXmlData = new QAction( i18nc("@ịnfo:action", "Show XML Requests/Replies"), menu );
    QAction *showTextData = new QAction( i18nc("@ịnfo:action", "Show Text Requests/Replies"), menu );
    QAction *showImageData = new QAction( i18nc("@ịnfo:action", "Show Image Requests/Replies"), menu );
    QAction *showCssData = new QAction( i18nc("@ịnfo:action", "Show CSS Requests/Replies"), menu );
    QAction *showScriptData = new QAction( i18nc("@ịnfo:action", "Show Script Requests/Replies"), menu );
    QAction *showUnknownData = new QAction( i18nc("@ịnfo:action", "Show Unknown Requests/Repliess"), menu );
    showHtmlData->setCheckable( true );
    showXmlData->setCheckable( true );
    showTextData->setCheckable( true );
    showImageData->setCheckable( true );
    showCssData->setCheckable( true );
    showScriptData->setCheckable( true );
    showUnknownData->setCheckable( true );

    NetworkMonitorModelItem::ContentTypes contentTypes = m_filterModel->contentTypeFilter();
    showHtmlData->setChecked( contentTypes.testFlag(NetworkMonitorModelItem::HtmlData) );
    showXmlData->setChecked( contentTypes.testFlag(NetworkMonitorModelItem::XmlData) );
    showTextData->setChecked( contentTypes.testFlag(NetworkMonitorModelItem::UnknownTextData) );
    showImageData->setChecked( contentTypes.testFlag(NetworkMonitorModelItem::ImageData) );
    showCssData->setChecked( contentTypes.testFlag(NetworkMonitorModelItem::CssData) );
    showScriptData->setChecked( contentTypes.testFlag(NetworkMonitorModelItem::ScriptData) );
    showUnknownData->setChecked( contentTypes.testFlag(NetworkMonitorModelItem::UnknownData) );

    QActionGroup *contentTypeGroup = new QActionGroup( menu );
    contentTypeGroup->addAction( showHtmlData );
    contentTypeGroup->addAction( showXmlData );
    contentTypeGroup->addAction( showTextData );
    contentTypeGroup->addAction( showImageData );
    contentTypeGroup->addAction( showCssData );
    contentTypeGroup->addAction( showScriptData );
    contentTypeGroup->addAction( showUnknownData );
    contentTypeGroup->setExclusive( false );
    actions << separator << contentTypeGroup->actions();

    QAction *separator2 = new QAction( menu );
    separator2->setSeparator( true );
    QAction *showGetRequests = new QAction( i18nc("@ịnfo:action", "Show GET Requests"), menu );
    QAction *showPostRequests = new QAction( i18nc("@ịnfo:action", "Show POST Requests"), menu );
    QAction *showReplies = new QAction( i18nc("@ịnfo:action", "Show Replies"), menu );
    showGetRequests->setCheckable( true );
    showPostRequests->setCheckable( true );
    showReplies->setCheckable( true );

    NetworkMonitorModelItem::Types types = m_filterModel->typeFilter();
    showGetRequests->setChecked( types.testFlag(NetworkMonitorModelItem::GetRequest) );
    showPostRequests->setChecked( types.testFlag(NetworkMonitorModelItem::PostRequest) );
    showReplies->setChecked( types.testFlag(NetworkMonitorModelItem::Reply) );

    QActionGroup *typeGroup = new QActionGroup( menu );
    typeGroup->addAction( showGetRequests );
    typeGroup->addAction( showPostRequests );
    typeGroup->addAction( showReplies );
    typeGroup->setExclusive( false );
    actions << separator2 << typeGroup->actions();

    menu->addActions( actions );
    QAction *action = menu->exec( m_widget->mapToGlobal(pos) );
    if ( action ) {
        if ( action == copyAction ) {
            const QVariant editData = index.data( Qt::EditRole );
            if ( editData.canConvert<QPixmap>() ) {
                QApplication::clipboard()->setPixmap( editData.value<QPixmap>() );
            } else {
                QApplication::clipboard()->setText( editData.toString() );
            }
        } else if ( action == clearAction ) {
            m_widget->model()->removeRows( 0, m_widget->model()->rowCount() );
        } else {
            NetworkMonitorModelItem::Types types = NetworkMonitorModelItem::Invalid;
            if ( showGetRequests->isChecked() ) {
                types |= NetworkMonitorModelItem::GetRequest;
            }
            if ( showPostRequests->isChecked() ) {
                types |= NetworkMonitorModelItem::PostRequest;
            }
            if ( showReplies->isChecked() ) {
                types |= NetworkMonitorModelItem::Reply;
            }
            m_filterModel->setTypeFilter( types );

            NetworkMonitorModelItem::ContentTypes contentTypes = NetworkMonitorModelItem::NoData;
            if ( showHtmlData->isChecked() ) {
                contentTypes |= NetworkMonitorModelItem::HtmlData;
            }
            if ( showXmlData->isChecked() ) {
                contentTypes |= NetworkMonitorModelItem::XmlData;
            }
            if ( showTextData->isChecked() ) {
                contentTypes |= NetworkMonitorModelItem::UnknownTextData;
            }
            if ( showImageData->isChecked() ) {
                contentTypes |= NetworkMonitorModelItem::ImageData;
            }
            if ( showCssData->isChecked() ) {
                contentTypes |= NetworkMonitorModelItem::CssData;
            }
            if ( showScriptData->isChecked() ) {
                contentTypes |= NetworkMonitorModelItem::ScriptData;
            }
            if ( showUnknownData->isChecked() ) {
                contentTypes |= NetworkMonitorModelItem::UnknownData;
            }
            m_filterModel->setContentTypeFilter( contentTypes );
        }
    }
    delete menu.data();
}

void NetworkMonitorDockWidget::activeProjectAboutToChange( Project *project,
                                                           Project *previousProject )
{
    if ( previousProject ) {
        if ( previousProject->webTab() ) {
            disconnect( previousProject->webTab(), SIGNAL(destroyed(QObject*)),
                        this, SLOT(tabClosed(QObject*)) );
        }
        disconnect( previousProject, SIGNAL(tabOpenRequest(AbstractTab*)),
                    this, SLOT(tabOpenRequest(AbstractTab*)) );
    }

    QAbstractItemModel *oldModel = m_filterModel->sourceModel();
    if ( project ) {
        if ( project->webTab() ) {
            m_filterModel->setSourceModel( project->webTab()->networkMonitorModel() );
            connect( project->webTab(), SIGNAL(destroyed(QObject*)),
                     this, SLOT(tabClosed(QObject*)) );
            initModel();
        } else {
            m_filterModel->setSourceModel( new NetworkMonitorModel(this) );
        }
        connect( project, SIGNAL(tabOpenRequest(AbstractTab*)),
                 this, SLOT(tabOpenRequest(AbstractTab*)) );
    } else {
        m_filterModel->setSourceModel( new NetworkMonitorModel(this) );
    }

    if ( oldModel && oldModel->QObject::parent() == this ) {
        delete oldModel;
    }
}

void NetworkMonitorDockWidget::tabOpenRequest( AbstractTab *tab )
{
    WebTab *webTab = qobject_cast< WebTab* >( tab );
    if ( webTab ) {
        QAbstractItemModel *oldModel = m_filterModel->sourceModel();
        m_filterModel->setSourceModel( webTab->networkMonitorModel() );
        connect( webTab, SIGNAL(destroyed(QObject*)),
                 this, SLOT(tabClosed(QObject*)) );
        initModel();
        if ( oldModel && oldModel->QObject::parent() == this ) {
            delete oldModel;
        }
    }
}

void NetworkMonitorDockWidget::tabClosed( QObject *tab )
{
    Q_UNUSED( tab );
    QAbstractItemModel *oldModel = m_filterModel->sourceModel();
    m_filterModel->setSourceModel( new NetworkMonitorModel(this) );
    if ( oldModel && oldModel->QObject::parent() == this ) {
        delete oldModel;
    }
}

void NetworkMonitorDockWidget::initModel()
{
    // Initialize header
    QHeaderView *header = m_widget->header();
    header->setDefaultSectionSize( 300 );
    header->setResizeMode( static_cast<int>(NetworkMonitorModel::TypeColumn), QHeaderView::ResizeToContents );
    header->setResizeMode( static_cast<int>(NetworkMonitorModel::TimeColumn), QHeaderView::ResizeToContents );
    header->setResizeMode( static_cast<int>(NetworkMonitorModel::ContentTypeColumn), QHeaderView::ResizeToContents );
    header->setResizeMode( static_cast<int>(NetworkMonitorModel::UrlColumn), QHeaderView::Interactive );
    header->setResizeMode( static_cast<int>(NetworkMonitorModel::DataColumn), QHeaderView::Stretch );
}
