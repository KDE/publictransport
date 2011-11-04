/*
    Copyright (C) 2011  Friedrich Pülz <fieti1983@gmx.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/** @file
 * @brief This file contains a QListView for journey searches, using a custom delegate.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef JOURNEYSEARCHLISTVIEW_H
#define JOURNEYSEARCHLISTVIEW_H

// Base class includes
#include <QListView> // Base class
#include <QStyledItemDelegate> // Base class
#include <QToolButton> // Base class

class KAction;

/**
 * @brief A QListView for journey searches with a context menu.
 *
 * This view is intended to be used with JourneySearchModel.
 * It offers a context menu with actions like adding/removing/editing journey searches and
 * toggling their favorite states. To use the context menu the used model must be of type
 * JourneySearchModel for simplicity.
 * JourneySearchDelegate is set as the item delegate automatically.
 *
 * @see JourneySearchModel
 **/
class JourneySearchListView : public QListView {
    Q_OBJECT

public:
    /** @brief Creates a new JourneySearchListView with the given @p parent. */
    explicit JourneySearchListView( QWidget *parent = 0 );

public slots:
    /** @brief Adds a new empty journey search item. */
    void addJourneySearch();

    /** @brief Removes the currently selected journey search item. */
    void removeCurrentJourneySearch();

    /** @brief Starts the edit mode for the currently selected journey search item. */
    void editJourneySearchAction();

    /** @brief Toggles the favorite state of the currently selected journey search item. */
    void toggleFavorite();

protected:
    /** @brief Overridden to create a custom context menu */
    virtual void contextMenuEvent( QContextMenuEvent *event );

private:
    KAction *m_addAction;
    KAction *m_removeAction;
    KAction *m_editAction;
    KAction *m_toggleFavoriteAction;
};

/**
 * @brief An item delegate for journey search items, eg. in a JourneySearchListView.
 *
 * This delegate implements editor functionality using a ToggleIconToolButton to configure the
 * favorite state and KLineEdit's to configure the name and the journey search string.
 **/
class JourneySearchDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    /** @brief Creates a new JourneySearchDelegate with the given @p parent. */
    explicit JourneySearchDelegate( QObject *parent = 0 );

protected:
    virtual QSize sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const;
    virtual void paint( QPainter *painter, const QStyleOptionViewItem &option,
                        const QModelIndex &index ) const;
    virtual QWidget* createEditor( QWidget *parent, const QStyleOptionViewItem &option,
                                   const QModelIndex &index ) const;
    virtual void setEditorData( QWidget *editor, const QModelIndex &index ) const;
    virtual void setModelData( QWidget *editor, QAbstractItemModel *model,
                               const QModelIndex &index ) const;
    virtual void updateEditorGeometry( QWidget *editor, const QStyleOptionViewItem &option,
                                       const QModelIndex &index ) const;
};

/**
 * @brief A QToolButton which displays journey search favorite/recent icons.
 *
 * This tool button uses JourneySearchModel::favoriteIconPixmap() with isChecked() as argument
 * to get the icon to draw. It does not draw any frame.
 **/
class ToggleIconToolButton : public QToolButton {
    Q_OBJECT

public:
    /** @brief Creates a new ToggleIconToolButton with the given @p parent. */
    explicit ToggleIconToolButton( QWidget *parent = 0 ) : QToolButton(parent) {};

protected:
    /** @brief Overridden to draw the correct icon, depending on the checked state. */
    virtual void paintEvent( QPaintEvent *event );
};

#endif // FAVORITEJOURNEYSEARCHWIDGET_H
