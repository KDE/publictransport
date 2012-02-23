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

#ifndef TIMETABLEMATEVIEW_H
#define TIMETABLEMATEVIEW_H

// Own includes
#include "ui_timetablemateview_base.h"

// PublicTransport engine includes
#include <engine/accessorinfoxmlreader.h>

// Qt includes
#include <QtGui/QWidget>

class ChangelogWidget;
class TimetableAccessor;
class QSignalMapper;
class QPainter;
class KUrl;

/**
 * This is the main view class for TimetableMate.  Most of the non-menu,
 * non-toolbar, and non-statusbar (e.g., non frame) GUI code should go
 * here.
 *
 * @short Main view
 * @author Friedrich Pülz fpuelz@gmx.de
 * @version 0.2
 */
class TimetableMateView : public QWidget, public Ui::timetablemateview_base
{
    Q_OBJECT
public:
    /** Default constructor */
    TimetableMateView( QWidget *parent );

    /** Destructor */
    virtual ~TimetableMateView();

    bool readAccessorInfoXml( const QString &fileName, QString *error = 0 );
    bool readAccessorInfoXml( QIODevice *device, QString *error/* = 0*/,
                              const QString &fileName/* = QString()*/ );

    bool writeAccessorInfoXml( const QString &fileName );
    QString writeAccessorInfoXml();

    TimetableAccessor *accessor() const;
    void setScriptFile( const QString &scriptFile );

    void setCurrentServiceProviderID( const QString &currentServiceProviderID ) {
        m_currentServiceProviderID = currentServiceProviderID;
    };

signals:
    /** Some widgets value has been changed. */
    void changed();
    void fileVersionchanged();

    /** A new script file has been created. */
    void scriptAdded( const QString &scriptFile );
    /** The used script file has changed. */
    void scriptFileChanged( const QString &scriptFile );

    void urlShouldBeOpened( const QString &url );

    /** Use this signal to change the content of the statusbar */
    void signalChangeStatusbar( const QString &text );

    /** Use this signal to change the content of the caption */
    void signalChangeCaption( const QString &text );

private slots:
    void settingsChanged();

    void browseForScriptFile();
    void createScriptFile();
    void detachScriptFile();

    void slotChanged( QWidget *changedWidget );
    void languageActivated( const QString &languageCode );

    void openUrlClicked();

    void predefinedCityNameChanged( const QString &newCityName );
    void predefinedCityReplacementChanged( const QString &newReplacement );
    void currentPredefinedCityChanged( const QString &currentCityText );

private:
    void fillValuesFromWidgets();

    Ui::timetablemateview_base ui_accessor;
    QString m_openedPath;
    QString m_currentServiceProviderID;
    TimetableAccessor *m_accessor;

    KEditListBox::CustomEditor m_predefinedCitiesCustomEditor;
    KLineEdit *m_cityName;
    KLineEdit *m_cityReplacement;
    ChangelogWidget *m_changelog;

    QSignalMapper *m_mapper;
};

#endif // TimetableMateVIEW_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
