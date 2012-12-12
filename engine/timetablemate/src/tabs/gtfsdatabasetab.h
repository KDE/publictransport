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

#ifndef GTFSDATABASETAB_H
#define GTFSDATABASETAB_H

#include "abstracttab.h"
#include "../project.h"

class QSqlQueryModel;
class KTextEdit;
class KJob;
class KTabWidget;
class KComboBox;
class QSqlTableModel;
class QTableView;
class QDeclarativeView;

/** @brief Represents a GTFS database tab. */
class GtfsDatabaseTab : public AbstractTab {
    Q_OBJECT
public:
    virtual ~GtfsDatabaseTab();

    static GtfsDatabaseTab *create( Project *project, QWidget *parent );
    virtual inline TabType type() const { return Tabs::GtfsDatabase; };

    QSqlTableModel *model() const { return m_model; };
    QDeclarativeView *qmlView() const { return m_qmlView; };

protected slots:
    void tableChosen( int index );

    void gtfsDatabaseStateChanged( Project::GtfsDatabaseState state );
    void executeQuery();

private:
    GtfsDatabaseTab( Project *project, QWidget *parent = 0 );

    QSqlTableModel *m_model;
    QSqlQueryModel *m_queryModel;
    KTabWidget *m_tabWidget;
    QDeclarativeView *m_qmlView;
    KComboBox *m_tableChooser;
    QTableView *m_tableView;
    QTableView *m_queryTableView;
    KTextEdit *m_query;
};

#endif // Multiple inclusion guard
