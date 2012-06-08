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

#ifndef PROJECTTABS_HEADER
#define PROJECTTABS_HEADER

// Own includes
#include "enums.h"

// Qt includes
#include <QWidget>

class Project;
class TimetableAccessorInfo;
class ProjectSettingsDialog;
class PlasmaPreview;
class JavaScriptCompletionModel;

class KAction;
class KIcon;
class KComboBox;
class KUrlComboBox;
class KWebView;
namespace KTextEditor
{
    class Document;
    class View;
}

class QUrl;
class QTimer;
class QSortFilterProxyModel;
class QWidget;
class QMenu;

/** @brief Base class for TimetableMate tabs. */
class AbstractTab : public QWidget {
    Q_OBJECT
public:
    /** @brief Destructor. */
    virtual ~AbstractTab();

    /** @brief Get the type of this tab. */
    virtual TabType type() const = 0;

    /** @brief Whether or not this is a dashboard tab. */
    inline bool isDashboardTab() const { return type() == Tabs::Dashboard; };

    /** @brief Whether or not this is an accessor document tab. */
    inline bool isAccessorDocumentTab() const { return type() == Tabs::ProjectSource; };

    /** @brief Whether or not this is a script document tab. */
    inline bool isScriptTab() const { return type() == Tabs::Script; };

    /** @brief Whether or not this is a web tab. */
    inline bool isWebTab() const { return type() == Tabs::Web; };

    /** @brief Whether or not this is a plasma preview tab. */
    inline bool isPlasmaPreviewTab() const { return type() == Tabs::PlasmaPreview; };

    /** @brief Get a short name for this tab type (not to be displayed). */
    QLatin1String typeName() const;

    /** @brief Get the title of this tab. */
    QString title() const;

    /**
     * @brief Get the icon of this tab.
     *
     * The default implementation creates an icon based on the type of this tab.
     **/
    virtual QIcon icon() const;

    /** @brief Get the project this tab belongs to. */
    inline Project *project() const { return m_project; };

    /** @brief Get the content widget of this tab. */
    inline QWidget *widget() const { return m_widget; };

    /** @brief Whether or not the tab has modified contents. */
    inline bool isModified() const { return m_modified; };

    /**
     * @brief Get the file name where the tab contents are saved or a placeholder string.
     *
     * The default implementation always returns a translated placeholder string.
     **/
    virtual QString fileName();

    /** @brief Show a context menu for this tab at @p globalPos. */
    void showTabContextMenu( const QPoint &globalPos );

signals:
    /** @brief Modified state has changed to @p isChanged. */
    void modifiedStatusChanged( bool isChanged );

    /** @brief Emitted whenever the contents of this tab have changed. */
    void changed();

    /** @brief The title of the tab has changed to @p title. */
    void titleChanged( const QString &title );

    /** @brief Emitted if a message should be shown in the status bar. */
    void statusBarMessage( const QString &message );

    /** @brief Emitted when this tab should be closed. */
    void tabCloseRequest();

    /** @brief Emitted when all other tabs should be closed. */
    void otherTabsCloseRequest();

public slots:
    /**
     * @brief Save tab contents. The default implementation does nothing and return true.
     * @return True on success, false otherwise.
     **/
    virtual bool save() { return true; };

    /** @brief The title of the tab has changed. */
    void slotTitleChanged() { emit titleChanged(title()); };

protected slots:
    void setModified( bool modified = true );

protected:
    /** @brief Create a new tab object for @p project. */
    AbstractTab( Project *project, TabType type, QWidget *parent = 0 );

    /** @brief Sets the widget shown in this tab page. */
    void setWidget( QWidget *widget );

    virtual QList< QAction* > contextMenuActions( QWidget *parent = 0 ) const;

private:
    Project *const m_project;
    QWidget *m_widget;
    bool m_modified;
};

/**
 * @brief Base class for tabs showing a KTextEditor::Document.
 *
 * Use createDocument() to create the KTextEditor::Document. Then use setDocument() to set the
 * used document. The modified state of this tab is automatically synchronized with the modified
 * state of the document, by connecting to the modifiedChanged() signal.
 **/
class AbstractDocumentTab : public AbstractTab {
    Q_OBJECT
public:
    /** @brief Get a pointer to the KTextEditor document used in this tab. */
    inline KTextEditor::Document *document() const { return m_document; };

    /**
     * @brief Get a pointer to the view shown in this tab.
     * If no view was created it gets created here.
     **/
    KTextEditor::View *defaultView() const;

    /**
     * @brief Get the file name where the tabs document is saved.
     *
     * The default implementation uses url() on the document to get the file name. If no file
     * is loaded a translated placeholder string gets returned.
     **/
    virtual QString fileName();

protected slots:
    void slotModifiedChanged( KTextEditor::Document *document );

    /** @brief Reimplemented to load the custom xmlgui file for the view. */
    void viewCreated( KTextEditor::Document *document, KTextEditor::View *view );

protected:
    /** @brief Create a new document tab object for @p project. */
    AbstractDocumentTab( Project *project, KTextEditor::Document *document,
                         TabType type, QWidget *parent = 0 );

    virtual ~AbstractDocumentTab();

    /** @brief Create a new KTextEditor::Document. */
    static KTextEditor::Document *createDocument( QWidget *parent );

    virtual QList< QAction* > contextMenuActions( QWidget *parent ) const;

private:
    KTextEditor::Document *const m_document;
};

#endif // Multiple inclusion guard
