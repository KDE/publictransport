/*
*   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef CHANGELOGENTRYWIDGET_HEADER
#define CHANGELOGENTRYWIDGET_HEADER

// PublicTransport engine includes
#include "engine/accessorinfoxmlreader.h"

// libpublictransporthelper includes
#include <dynamicwidget.h>

// Qt includes
#include <QWidget>

class KTextBrowser;
class KLineEdit;

/** @file
* @brief Contains ChangelogEntryWidget and ChangelogWidget.
*
* @author Friedrich Pülz <fpuelz@gmx.de> */

/**
 * @brief Shows a changelog entry in editable widgets.
 **/
class ChangelogEntryWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates a new changelog entry widget.
     *
     * @param parent The parent widget of the changelog entry widget. Default is 0.
     **/
    explicit ChangelogEntryWidget( QWidget *parent = 0,
                                   const ChangelogEntry &changelogEntry = ChangelogEntry(),
                                   const QString &shortAuthor = QString() );

    QString author() const;
    QString version() const;
    QString releasedWith() const;
    QString description() const;
    ChangelogEntry changelogEntry() const;

    void setAuthor( const QString &author );
    void setVersion( const QString &version );
    void setReleasedWith( const QString &releasedWith );
    void setDescription( const QString &description );
    void setChangelogEntry( const ChangelogEntry &changelogEntry, const QString &shortAuthor );

    KLineEdit *authorLineEdit() const {
        return m_author;
    };
    KLineEdit *versionLineEdit() const {
        return m_version;
    };
    KLineEdit *releasedWithLineEdit() const {
        return m_releasedWith;
    };
    KLineEdit *descriptionLineEdit() const {
        return m_description;
    };

signals:
    void changed();

private:
    KLineEdit *m_author;
    KLineEdit *m_version;
    KLineEdit *m_releasedWith;
    KLineEdit *m_description;
};

/** @brief Manages a list of @ref ChangelogEntryWidget in a widget, with buttons to dynamically
 *  add/remove ChangelogEntryWidgets. */
class ChangelogWidget : public AbstractDynamicWidgetContainer
{
    Q_OBJECT

public:
    explicit ChangelogWidget( QWidget *parent = 0,
                              RemoveButtonOptions removeButtonOptions = RemoveButtonsBesideWidgets,
                              AddButtonOptions addButtonOptions = AddButtonAfterLastWidget,
                              SeparatorOptions separatorOptions = ShowSeparators );

    void addChangelogEntry( const ChangelogEntry &changelogEntry, const QString &shortAuthor );
    void addChangelog( const QList<ChangelogEntry> &changelog, const QString &shortAuthor );
    void clear();

    QList<ChangelogEntry> changelog() const;
    QList<ChangelogEntryWidget *> entryWidgets() const {
        return AbstractDynamicWidgetContainer::widgets<ChangelogEntryWidget *>();
    };

signals:
    void changed();

public slots:
    void createAndAddWidgetSameVersion();
    void createAndAddWidgetNewMinorVersion();
    void createAndAddWidgetNewMajorVersion();

protected:
    virtual QWidget *createNewWidget();
    virtual DynamicWidget *addWidget( QWidget *widget );
};

#endif // CHANGELOGENTRYWIDGET_HEADER
// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
