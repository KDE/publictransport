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
 * @brief This file contains a model for journey searches.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef JOURNEYSEARCHMODEL_H
#define JOURNEYSEARCHMODEL_H

// Own includes
#include "journeysearchitem.h"

// Qt includes
#include <QAbstractItemModel>
#include <QIcon>

class JourneySearchModel;

/**
 * @brief An item containing information about journey searches, used by JourneySearchModel.
 *
 * This class gets used by JourneySearchModel, you can not create objects youself because there
 * is no public constructor. Pointers to instances of this class can be retrieved from
 * JourneySearchModel.
 **/
class JourneySearchModelItem : public JourneySearchItem {
    friend class JourneySearchModel;

public:
    /** @brief Gets the model of this item. */
    JourneySearchModel *model() const { return m_model; };

    /** @brief Gets the model index of this item. */
    QModelIndex index() const;

    /** @brief Overridden from JourneySearchItem, notifies the model about the change. */
    virtual inline void setFavorite( bool favorite ) {
        JourneySearchItem::setFavorite( favorite );
        dataChanged();
    };

    /** @brief Overridden from JourneySearchItem, notifies the model about the change. */
    virtual inline void setJourneySearch( const QString &journeySearch ) {
        JourneySearchItem::setJourneySearch( journeySearch );
        dataChanged();
    };

    /** @brief Overridden from JourneySearchItem, notifies the model about the change. */
    virtual inline void setName( const QString &name ) {
        JourneySearchItem::setName( name );
        dataChanged();
    };

protected:
    /**
     * @brief Creates a new journey search item associated with @p model.
     *
     * @param model The model in which this journey search item is.
     * @param journeySearch The journey search string to associate with this journey search item.
     * @param name The name to be used as alias for @p journeySearch.
     * @param favorite Whether or not @p journeySearch is a favorite journey search.
     *   Defaults to false.
     **/
    JourneySearchModelItem( JourneySearchModel *model, const QString &journeySearch,
                            const QString &name = QString(), bool favorite = false );

private:
    /** @brief Notifies the associated JourneySearchModel about a change in this item. */
    void dataChanged() const;

    JourneySearchModel *m_model;
};

/**
 * @brief A model containing journey search string.
 *
 * Journey search strings are represented by JourneySearchModelItem's.
 * The sort() function groups favorite and non-favorite journey search items.
 **/
class JourneySearchModel : public QAbstractListModel
{
    Q_OBJECT
    friend class JourneySearchModelItem;

public:
    /** @brief Additional roles used by this model. */
    enum Roles {
        JourneySearchRole = Qt::UserRole + 1, /**< Contains the journey search string. */
        FavoriteRole = Qt::UserRole + 2, /**< Contains a boolean, which is true if the associated
                * journey search is a favorite. */
        NameRole = Qt::UserRole + 3 /**< Contains the name to be used as alias for the journey
                * search string (JourneySearchRole). */
    };

    /**
     * @brief Creates a new journey search model.
     *
     * @param parent The parent of this model. Default is 0.
     **/
    explicit JourneySearchModel( QObject *parent = 0 );

    /**
     * @brief Destructor.
     **/
    virtual ~JourneySearchModel() {
        qDeleteAll( m_items );
    };

    /**
     * @brief Gets the icon to be used for items of this model.
     *
     * @param favorite If this is true, the icon for favorite journey search items gets returned.
     *   Otherwise, the icon for non-favorite items gets returned. Default is true.
     * @see favoriteIconPixmap
     **/
    static QIcon favoriteIcon( bool favorite = true );

    /**
     * @brief Gets a pixmap of the icon to be used for items of this model.
     *
     * @param favorite If this is true, the icon for favorite journey search items gets returned.
     *   Otherwise, the icon for non-favorite items gets returned. Default is true.
     * @see favoriteIcon
     **/
    static QPixmap favoriteIconPixmap( bool favorite = true );

    /** @brief Gets the item at @p index. */
    JourneySearchModelItem *item( const QModelIndex &index );

    /** @brief Gets the item with the given @p journeySearch. */
    inline JourneySearchModelItem *item( const QString &journeySearch ) {
        return item( indexFromJourneySearch(journeySearch) );
    };

    /**
     * @brief Add @p journeySearch to the model.
     *
     * @param journeySearch The new journey search string to add to the model.
     * @param name The name to be used as alias for @p journeySearch.
     * @param favorite Whether or not the new journey search item is a favorite. Default is false.
     * @return JourneySearchItem* The newly added journey search item.
     **/
    JourneySearchModelItem *addJourneySearch( const QString &journeySearch,
            const QString &name = QString(), bool favorite = false );

    /**
     * @brief Add @p journeySearch to the model.
     *
     * This is an overloaded function provided for convenience.
     *
     * @param item An item with information about the journey search.
     * @return JourneySearchModelItem* The newly added journey search item.
     **/
    inline JourneySearchModelItem *addJourneySearch( const JourneySearchModelItem &item ) {
        return addJourneySearch( item.journeySearch(), item.name(), item.isFavorite() );
    };

    /** @brief Removes the journey search item at @p index and returns true on success. */
    bool removeJourneySearch( const QModelIndex &index );

    /**
     * @brief Removes @p journeySearch from the model and returns true on success.
     *
     * This is an overloaded function provided for convenience.
     **/
    inline bool removeJourneySearch( const QString &journeySearch ) {
        return removeJourneySearch( indexFromJourneySearch(journeySearch) );
    };

    /** @brief Clears the model, ie. removes all journey search items. */
    void clear();

    /** @brief Gets the QModelIndex of the item with the given @p journeySearch. */
    QModelIndex indexFromJourneySearch( const QString &journeySearch );

    /** @brief Gets the QModelIndex of @p item. */
    inline QModelIndex indexFromItem( const JourneySearchModelItem *item ) {
        return indexFromJourneySearch( item->journeySearch() );
    };

    /**
     * @brief Gets the data for the given @p index and @p role.
     **/
    virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;

    /**
     * @brief Gets the number of rows in this model.
     *
     * @param parent Isn't used, because this model has no subitems.
     *   If a valid parent index is given, 0 is returned. Defaults to QModelIndex().
     * @return The number of rows in this model.
     **/
    virtual int rowCount( const QModelIndex& parent = QModelIndex() ) const {
        return parent.isValid() ? 0 : m_items.count();
    };

    /**
     * @brief Gets an index for the given @p row and @p column. @p parent isn't used.
     **/
    virtual QModelIndex index( int row, int column,
                               const QModelIndex& parent = QModelIndex() ) const;

    /** @brief Overridden from base class. */
    virtual bool insertRows( int row, int count, const QModelIndex &parent = QModelIndex() );

    /** @brief Overridden from base class. */
    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());

    /** @brief Overridden from base class. */
    virtual bool setData( const QModelIndex &index, const QVariant &value,
                          int role = Qt::EditRole );

    /** @brief Overridden from base class. */
    virtual bool setItemData( const QModelIndex &index, const QMap< int, QVariant > &roles );

    /** @brief Gets flags for the items of this model. */
    virtual Qt::ItemFlags flags( const QModelIndex& ) const {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    };

    /**
     * @brief Sorts the items of this model. Overriden from base class.
     *
     * Use sort() instead to use the only supported parameters.
     *
     * @param column Only 0 is supported here.
     * @param order Only Qt::AscendingOrder is supported here.
     **/
    virtual void sort( int column, Qt::SortOrder order = Qt::AscendingOrder );

    /**
     * @overload JourneySearchModel::sort( int column, Qt::SortOrder order = Qt::AscendingOrder )
     **/
    inline void sort() { sort(0, Qt::AscendingOrder); };


    /** @brief Gets a list of JourneySearchItem's. */
    QList<JourneySearchItem> journeySearchItems();

protected:
    void emitDataChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight );
    virtual bool setDataWithoutNotify( JourneySearchModelItem *item, const QVariant &value,
                                       int role = Qt::EditRole );
    static QPixmap favoriteIconPixmap( const QIcon &icon, bool favorite = true );

private:
    QList<JourneySearchModelItem*> m_items;
};

#endif // Multiple inclusion guard
