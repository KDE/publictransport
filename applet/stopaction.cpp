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

#include "stopaction.h"

#include <KIcon>
#include <KLocalizedString>

StopAction::StopAction( StopAction::Type type, QObject* parent, StopAction::TitleType titleType,
                        const QString &stopName, const QString &stopNameShortened )
        : QAction(parent), m_type(type), m_titleType(titleType),
          m_stopName(stopName), m_stopNameShortened(stopNameShortened)
{
    switch ( type ) {
    case ShowDeparturesForStop:
        setIcon( KIcon("public-transport-stop") );
        switch ( titleType ) {
        case ShowActionNameOnly:
            setText( i18nc("@action:inmenu", "Show &Departures From This Stop") );
            break;
        case ShowStopNameOnly:
            setText( i18nc("@action:inmenu", "Show &Departures From '%1'", m_stopNameShortened) );
            break;
        case ShowActionNameAndStopName:
            setText( m_stopNameShortened );
            break;
        }
        break;
    case ShowStopInMap:
        setIcon( KIcon("marble") );
        switch ( titleType ) {
        case ShowActionNameOnly:
            setText( i18nc("@action:inmenu", "Show This Stop in a Map") );
            break;
        case ShowStopNameOnly:
            setText( i18nc("@action:inmenu", "Show '%1' in a Map", m_stopNameShortened) );
            break;
        case ShowActionNameAndStopName:
            setText( m_stopNameShortened );
            break;
        }
        break;
    case HighlightStop:
        setIcon( KIcon("edit-select") );
        switch ( titleType ) {
        case ShowActionNameOnly: // TODO "Unhighlight" if the stop is already highlighted
            setText( i18nc("@action:inmenu", "&Highlight This Stop") );
            break;
        case ShowStopNameOnly:
            setText( i18nc("@action:inmenu", "&Highlight '%1'", m_stopNameShortened) );
            break;
        case ShowActionNameAndStopName:
            setText( m_stopNameShortened );
            break;
        }
        break;
    case CreateFilterForStop:
        setIcon( KIcon("view-filter") );
        switch ( titleType ) {
        case ShowActionNameOnly:
            setText( i18nc("@action:inmenu", "&Create Filter 'Via This Stop'") );
            break;
        case ShowStopNameOnly:
            setText( i18nc("@action:inmenu", "&Create Filter 'Via %1'", m_stopNameShortened) );
            break;
        case ShowActionNameAndStopName:
            setText( m_stopNameShortened );
            break;
        }
        break;
    case CopyStopNameToClipboard:
        setIcon( KIcon("edit-copy") );
        switch ( titleType ) {
        case ShowActionNameOnly:
            setText( i18nc("@action:inmenu", "&Copy Stop Name") );
            break;
        case ShowStopNameOnly:
            setText( i18nc("@action:inmenu", "&Copy '%1'", m_stopNameShortened) );
            break;
        case ShowActionNameAndStopName:
            setText( m_stopNameShortened );
            break;
        }
        break;
    case RequestJourneysFromStop:
        setIcon( KIcon("edit-find") );
        switch ( titleType ) {
        case ShowActionNameOnly:
            setText( i18nc("@action:inmenu", "&Search Journeys From This Stop") );
            break;
        case ShowStopNameOnly:
            setText( i18nc("@action:inmenu", "&Search Journeys From '%1'", m_stopNameShortened) );
            break;
        case ShowActionNameAndStopName:
            setText( m_stopNameShortened );
            break;
        }
        break;
    case RequestJourneysToStop:
        setIcon( KIcon("edit-find") );
        switch ( titleType ) {
        case ShowActionNameOnly:
            setText( i18nc("@action:inmenu", "&Search Journeys to This Stop") );
            break;
        case ShowStopNameOnly:
            setText( i18nc("@action:inmenu", "&Search Journeys to '%1'", m_stopNameShortened) );
            break;
        case ShowActionNameAndStopName:
            setText( m_stopNameShortened );
            break;
        }
        break;
    }

    connect( this, SIGNAL(triggered()), this, SLOT(slotTriggered()) );
}

void StopAction::slotTriggered()
{
    emit stopActionTriggered( m_type, m_stopName, m_stopNameShortened );
}
