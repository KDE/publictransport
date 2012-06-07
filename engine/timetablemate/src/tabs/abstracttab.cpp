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
#include "abstracttab.h"

// Own includes
#include "project.h"
#include "publictransportpreview.h"
#include "parserenums.h"
#include "javascriptmodel.h"
#include "javascriptparser.h"
#include "javascriptcompletionmodel.h"
#include "../debugger/debugger.h"

// Public Transport engine include
#include <engine/timetableaccessor_info.h>
#include <engine/timetableaccessor.h>

// KDE includes
#include <KWebView>
#include <KUrlComboBox>
#include <KAction>
#include <KActionMenu>
#include <KLocalizedString>
#include <KService>
#include <KStandardDirs>
#include <KTextEditor/Document>
#include <KTextEditor/ConfigInterface>
#include <KTextEditor/TextHintInterface>
#include <KTextEditor/MarkInterface>
#include <KTextEditor/CodeCompletionInterface>
#include <KTextEditor/View>

// Qt includes
#include <QVBoxLayout>
#include <QSplitter>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QWebInspector>
#include <QTimer>
#include <QFileInfo>

AbstractTab::AbstractTab( Project *project, TabType type, QWidget *parent )
        : QWidget(parent), m_project(project), m_widget(0), m_modified(false)
{
    // Cannot call typeName() here, because it uses type(), which is virtual
    setObjectName( Tabs::nameForType(type) );

    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );

    connect( this, SIGNAL(tabCloseRequest()), project, SLOT(slotTabCloseRequest()) );
    connect( this, SIGNAL(otherTabsCloseRequest()), project, SLOT(slotOtherTabsCloseRequest()) );
}

AbstractTab::~AbstractTab()
{
    if ( isModified() ) {
        kWarning() << "Destroying tab with modifications";
    }
}

AbstractDocumentTab::AbstractDocumentTab( Project *project, KTextEditor::Document *document,
                                        TabType type, QWidget *parent )
        : AbstractTab(project, type, parent), m_document(document)
{
    connect( m_document, SIGNAL(modifiedChanged(KTextEditor::Document*)),
             this, SLOT(slotModifiedChanged(KTextEditor::Document*)) );
    connect( m_document, SIGNAL(viewCreated(KTextEditor::Document*,KTextEditor::View*)),
             this, SLOT(viewCreated(KTextEditor::Document*,KTextEditor::View*)) );
}

AbstractDocumentTab::~AbstractDocumentTab()
{
    delete m_document;
}

void AbstractTab::setWidget( QWidget *widget )
{
    if ( m_widget ) {
        layout()->removeWidget( m_widget );
    }
    layout()->addWidget( widget );
    m_widget = widget;
    connect( m_widget, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()) );
}

KTextEditor::Document *AbstractDocumentTab::createDocument( QWidget *parent )
{
    // First check if the kate part can be found
    KService::Ptr service = KService::serviceByDesktopPath( "katepart.desktop" );
    if ( !service ) {
        return 0;
    }

    // Create text editor part
    return qobject_cast<KTextEditor::Document*>(
            service->createInstance<KParts::ReadWritePart>(parent) );
}

KTextEditor::View *AbstractDocumentTab::defaultView() const
{
    // Ensure a view gets created
    m_document->widget();

    if ( m_document->views().isEmpty() ) {
        kWarning() << "No view created";
        return 0;
    } else {
        return m_document->views().first();
    }
}

void AbstractDocumentTab::viewCreated( KTextEditor::Document *document, KTextEditor::View *view )
{
    Q_UNUSED( document );
    // The following code is copied from KDevelop (shell/textdocument.cpp), with a little change:
    // It merges with the the default katepartui.rc file. The only purpose of this code here is to
    // remove shortcuts which are also defined in timetablemateui.rc

    // in KDE >= 4.4 we can use KXMLGuiClient::replaceXMLFile to provide
    // katepart with our own restructured UI configuration
    const QString uiFile = KGlobal::mainComponent().componentName() + "/katepartui.rc";
    QStringList katePartUIs = KGlobal::mainComponent().dirs()->findAllResources("data", uiFile);
    if ( !katePartUIs.isEmpty() ) {
        const QString katePartUI = katePartUIs.last();
        const QString katePartLocalUI = KStandardDirs::locateLocal( "data", uiFile );
        if ( !QFile::exists(katePartLocalUI) ) {
            // prevent warning: No such XML file ".../.kde/share/apps/timetablemate/katepartui.rc"
            QFile::copy( katePartUI, katePartLocalUI );
        }
        view->replaceXMLFile( katePartUI, katePartLocalUI, true ); // Merge with global XML file
    }
}

void AbstractDocumentTab::slotModifiedChanged( KTextEditor::Document *document )
{
    setModified( document->isModified() );
}

void AbstractTab::setModified( bool modified )
{
    kDebug() << "Set Modified" << modified << m_project->projectName();
    if ( modified ) {
        emit changed();
    }

    if ( modified != m_modified ) {
        m_modified = modified;
        emit modifiedStatusChanged( modified );
        emit titleChanged( title() );
    }
}

QLatin1String AbstractTab::typeName() const
{
    return Tabs::nameForType( type() );
}

QString AbstractTab::fileName()
{
    return i18nc("@info/plain", "Unsaved Document");
}

QString AbstractDocumentTab::fileName()
{
    const KUrl url = document()->url();
    return url.isValid() ? url.path() : AbstractTab::fileName();
}

QIcon AbstractTab::icon() const
{
    if ( m_modified ) {
        return KIcon("document-save");
    } else {
        switch ( type() ) {
        case Tabs::Overview:
            return KIcon("zoom-draw"); // TODO Did not find a better icon for "overview", maybe create one
        case Tabs::ProjectSource:
            return KIcon("application-x-publictransport-serviceprovider");
        case Tabs::Script:
            return KIcon("application-javascript");
        case Tabs::Web:
            return KIcon("applications-internet");
        case Tabs::PlasmaPreview:
            return KIcon("plasma");
        default:
            return KIcon();
        }
    }
}

QString AbstractTab::title() const
{
    switch ( type() ) {
    case Tabs::Overview: {
        const QString name = m_project->projectName();
        return name.length() > 25 ? name.left(25) + QString::fromUtf8("…") : name;
    }
    case Tabs::ProjectSource: {
        const QString xmlFileName = m_project->filePath();
        return !xmlFileName.isEmpty() ? QFileInfo(xmlFileName).fileName()
                : i18nc("@title:tab", "Accessor Document %1", m_project->serviceProviderId());
    }
    case Tabs::Script: {
        const QString scriptFileName = m_project->accessor()->info()->scriptFileName() ;
        QString title = !scriptFileName.isEmpty() ? QFileInfo(scriptFileName).fileName()
                : i18nc("@title:tab", "Script %1", m_project->serviceProviderId());

        Debugger::Debugger *debugger = project()->debugger();
        if ( debugger->isRunning() ) {
            if ( debugger->hasUncaughtException() ) { //m_engine->hasUncaughtException() ) {
                title += " - " + i18nc("@info/plain", "Exception in Line %1",
                                       debugger->uncaughtExceptionLineNumber());
            } else if ( debugger->isInterrupted() ) {
                title += " - " + i18nc("@info/plain", "Interrupted at Line %1",
                                       debugger->lineNumber());
            } else if ( debugger->isRunning() ) {
                title += " - " + i18nc("@info/plain", "Running");
            }
        }
        return title;
    }
    case Tabs::Web:
        return i18nc("@title:tab", "Web %1", m_project->serviceProviderId());
    case Tabs::PlasmaPreview:
        return i18nc("@title:tab", "Plasma Preview %1", m_project->serviceProviderId());
    default:
        return "Unknown " + m_project->serviceProviderId();
    }
}

void AbstractTab::showTabContextMenu( const QPoint &globalPos )
{
    // Show context menu for this tab
    QPointer<QMenu> contextMenu( new QMenu(this) );
    contextMenu->addActions( contextMenuActions(contextMenu.data()) );

    contextMenu->exec( globalPos );
    delete contextMenu.data();
}

QList< QAction* > AbstractTab::contextMenuActions( QWidget *parent ) const
{
    KAction *closeTab = new KAction( KIcon("tab-close"), i18nc("@action", "Close Tab"), parent );
    connect( closeTab, SIGNAL(triggered(bool)), this, SIGNAL(tabCloseRequest()) );
    KAction *closeOtherTabs = new KAction( KIcon("tab-close-other"),
                                           i18nc("@action", "Close Other Tabs"), parent );
    connect( closeOtherTabs, SIGNAL(triggered(bool)), this, SIGNAL(otherTabsCloseRequest()) );

    KAction *separator = new KAction( parent );
    separator->setSeparator( true );

    return QList< QAction* >() << closeTab << closeOtherTabs
                               << separator << project()->projectSubMenuAction(parent);
}

QList< QAction* > AbstractDocumentTab::contextMenuActions( QWidget *parent ) const
{
    KAction *saveTabAction = new KAction( KIcon("document-save"),
                                          i18nc("@action", "Save Document"), parent );
    saveTabAction->setEnabled( isModified() );
    connect( saveTabAction, SIGNAL(triggered(bool)), this, SLOT(save()) );
    return QList< QAction* >() << saveTabAction << AbstractTab::contextMenuActions( parent );
}

