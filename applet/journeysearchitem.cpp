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

// Header include
#include "journeysearchitem.h"

// Own includes
#include "journeysearchmodel.h"

JourneySearchItem::JourneySearchItem( const QString &journeySearch, const QString &name,
                                      bool favorite )
    : m_journeySearch(journeySearch), m_name(name), m_favorite(favorite)
{

}

JourneySearchItem::JourneySearchItem() : m_favorite(false)
{

}

JourneySearchItem::JourneySearchItem( const JourneySearchItem &other )
    : m_journeySearch(other.m_journeySearch), m_name(other.m_name), m_favorite(other.m_favorite)
{

}

QIcon JourneySearchItem::icon() const
{
    return JourneySearchModel::favoriteIcon( isFavorite() );
}

bool JourneySearchItem::operator==( const JourneySearchItem &other ) const
{
    return m_favorite == other.m_favorite && m_name == other.m_name &&
           m_journeySearch == other.m_journeySearch;
}
