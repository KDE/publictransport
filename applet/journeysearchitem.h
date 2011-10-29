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

#ifndef JOURNEYSEARCHITEM_H
#define JOURNEYSEARCHITEM_H

// Qt includes
#include <QIcon>
#include <QMetaType>

/**
 * @brief An item containing information about journey searches.
 *
 * Can be used independently from JourneySearchModel, which uses a derived class
 * JourneySearchModelItem.
 *
 * JourneySearchItem's can be made favorite using setFavorite. Check if an item is a favorite
 * journey search using isFavorite.
 **/
class JourneySearchItem {
public:
    /**
     * @brief Creates a new journey search item.
     *
     * @param journeySearch The journey search string to associate with this journey search item.
     * @param name The name to be used as alias for @p journeySearch.
     * @param favorite Whether or not @p journeySearch is a favorite journey search.
     *   Defaults to false.
     **/
    explicit JourneySearchItem( const QString &journeySearch,
                                const QString &name = QString(), bool favorite = false );

    /**
     * @brief Creates an invalid journey search item.
     *
     * This constructor is needed for JourneySearchItem to work in QVariant's.
     **/
    JourneySearchItem();

    /** @brief Copy constructor. */
    JourneySearchItem( const JourneySearchItem &other );

    /** @brief Destructor. */
    virtual ~JourneySearchItem() {};

    /** @brief Gets the name to be used as alias for journeySearch() if not empty. */
    QString name() const { return m_name; };

    /** @brief Gets the journey search string associated with this journey search item. */
    QString journeySearch() const { return m_journeySearch; };

    /** @brief If name() is not empty it gets returned, otherwise journeySearch() gets returned. */
    QString nameOrJourneySearch() const { return m_name.isEmpty() ? m_journeySearch : m_name; };

    /** @brief Gets the icon for this item. */
    QIcon icon() const;

    /** @brief Whether or not this journey search item is a favorite journey search. */
    bool isFavorite() const { return m_favorite; };

    /**
     * @brief Sets whether or not this journey search item is a favorite journey search.
     *
     * @param favorite True, if this journey search item is a favorite journey search.
     *   False, otherwise.
     **/
    virtual void setFavorite( bool favorite ) { m_favorite = favorite; };

    /**
     * @brief Sets the journey search string associated with this journey search item.
     *
     * @param journeySearch The new journey search string.
     **/
    virtual void setJourneySearch( const QString &journeySearch )
            { m_journeySearch = journeySearch; };

    /**
     * @brief Sets the name to be used as alias for journeySearch().
     *
     * @param name The new name.
     **/
    virtual void setName( const QString &name ) { m_name = name; };

    /** @brief Compares this journey search item with @p other. */
    bool operator ==( const JourneySearchItem &other ) const;

private:
    QString m_journeySearch;
    QString m_name;
    bool m_favorite;
};

/** @brief Enable usage of JourneySearchItem's in QVariant's. */
Q_DECLARE_METATYPE( JourneySearchItem );

/** @brief Enable usage of QList's of JourneySearchItem's in QVariant's. */
Q_DECLARE_METATYPE( QList<JourneySearchItem> );

#endif // Multiple inclusion guard
