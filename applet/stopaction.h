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

#ifndef STOPACTION_HEADER
#define STOPACTION_HEADER

/** @file
 * Contains the StopAction class for actions with associated route stops.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#include <QAction> // Base class

class StopAction : public QAction
{
    Q_OBJECT
public:
    /** @brief Actions for intermediate stops, shown in RouteGraphicsItems. */
    enum Type {
        ShowDeparturesForStop,   /**< Show a departure list for the associated stop. */
        CreateFilterForStop,     /**< Create a filter via the associated stop. */
        CopyStopNameToClipboard, /**< Copy the name of the associated stop to the clipboard. */
        HighlightStop,           /**< Highlight the associated stop in all route items.
                                  * If the stop was already highlighted, it should
                                  * be unhighlighted. */
        RequestJourneysToStop,   /**< Request journeys to the associated stop. The origin stop
                                  * can be given as QVariant data argument to stop action requests. */
        RequestJourneysFromStop, /**< Request journeys from the associated stop. The target stop
                                  * can be given as QVariant data argument to stop action requests. */
        ShowStopInMap            /**< Show a map with the stop, eg. in a web browser. */
    };

    enum TitleType {
        ShowActionNameOnly,
        ShowStopNameOnly,
        ShowActionNameAndStopName
    };

    StopAction( Type type, QObject* parent, TitleType titleType = ShowActionNameOnly,
                const QString &stopName = QString(), const QString &stopNameShortened = QString() );

    Type type() const { return m_type; };
    TitleType titleType() const { return m_titleType; };

    QString stopName() const { return m_stopName; };
    QString stopNameShortened() const { return m_stopNameShortened; };
    void setStopName( const QString &stopName, const QString &stopNameShortened = QString() ) {
        m_stopName = stopName;
        m_stopNameShortened = stopNameShortened.isEmpty() ? stopName : stopNameShortened;
    };

signals:
    /**
     * @brief This signal gets fired when this QAction signals triggered(), but with more arguments.
     *
     * @param type The type of the triggered stop action.
     * @param stopName The name of the stop associated with the action.
     * @param stopNameShortened The shortened name of the stop associated with the action.
     **/
    void stopActionTriggered( StopAction::Type type,
                              const QString &stopName, const QString &stopNameShortened );

protected slots:
    void slotTriggered();

private:
    Q_DISABLE_COPY(StopAction)

    const Type m_type;
    const TitleType m_titleType;
    QString m_stopName;
    QString m_stopNameShortened;
};

#endif // STOPACTION_HEADER
