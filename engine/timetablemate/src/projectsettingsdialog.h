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

#ifndef PROJECTSETTINGSDIALOG_H
#define PROJECTSETTINGSDIALOG_H

// Own includes
#include "project.h"

// PublicTransport engine includes
#include <engine/serviceproviderdatareader.h>

// KDE includes
#include <KDialog>
#include <KEditListBox>

namespace Ui
{
    class timetablemateview_base;
}
class ServiceProvider;

class ChangelogEntryWidget;
class ChangelogWidget;
class QSignalMapper;
class QPainter;

class KUrl;
class KActionCollection;

typedef QSharedPointer< const ServiceProviderData > ServiceProviderDataPtr;

/**
 * @brief A dialog which allows to edit project settings.
 */
class ProjectSettingsDialog : public KDialog
{
    Q_OBJECT
public:
    /** Default constructor */
    ProjectSettingsDialog( QWidget *parent = 0 );

    /** Destructor */
    virtual ~ProjectSettingsDialog();

    const ServiceProviderData *providerData( QObject *parent ) const;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    Project::ScriptTemplateType newScriptTemplateType() const { return m_newScriptTemplateType; };
    void setScriptFile( const QString &scriptFile );
#endif

    void setProviderData( const ServiceProviderData *info, const QString &fileName = QString() );

    void setCurrentServiceProviderID( const QString &currentServiceProviderID ) {
        m_currentServiceProviderID = currentServiceProviderID;
    };

signals:
    /** Some widgets value has been changed or setProviderData() has been called. */
    void changed();

    void fileVersionchanged();

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /** A new script file has been created. */
    void scriptAdded( const QString &scriptFile );
    /** The used script file has changed. */
    void scriptFileChanged( const QString &scriptFile );
#endif

    void urlShouldBeOpened( const QString &url );

    /** Use this signal to change the content of the statusbar */
    void signalChangeStatusbar( const QString &text );

    /** Use this signal to change the content of the caption */
    void signalChangeCaption( const QString &text );

protected slots:
    void settingsChanged();

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    void browseForScriptFile();
    void createScriptFile();
    void detachScriptFile();
#endif

    void slotChanged( QWidget *changedWidget );
    void languageActivated( const QString &languageCode );

    void providerTypeChanged( int newProviderTypeIndex );
    void authorEdited( const QString &newAuthor );
    void shortAuthorEdited( const QString &shortAuthor );
    void urlEdited( const QString &newUrl );
    void shortUrlEdited( const QString &shortUrl );

    void openUrlClicked();

    void changelogEntryWidgetAdded( ChangelogEntryWidget *entryWidget );

    void predefinedCityNameChanged( const QString &newCityName );
    void predefinedCityReplacementChanged( const QString &newReplacement );
    void currentPredefinedCityChanged( const QString &currentCityText );

    virtual void slotButtonClicked( int button );
    virtual void accept();
    bool check();

protected:
    virtual bool eventFilter( QObject *object, QEvent *event );

private:
    void fillValuesFromWidgets();
    int compareVersions( const QString &version1, const QString &version2 );
    bool testWidget( QWidget *widget );
    void appendMessageWidgetAfter( QWidget *after, const QString &errorMessage );

    Enums::ServiceProviderType providerTypeFromComboBoxIndex( int index ) const;
    int providerTypeToComboBoxIndex( Enums::ServiceProviderType providerType ) const;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    QStringList scriptExtensionsFromWidget() const;
    void checkScriptExtensionsInWidget( const QStringList &scriptExtensions ) const;
#endif

    Ui::timetablemateview_base *ui_provider;

    QString m_openedPath;
    QString m_currentServiceProviderID;
    ServiceProviderData *m_providerData;
    bool m_shortAuthorAutoFilled;
    bool m_shortUrlAutoFilled;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    Project::ScriptTemplateType m_newScriptTemplateType;
#endif

    KEditListBox::CustomEditor m_predefinedCitiesCustomEditor;
    KLineEdit *m_cityName;
    KLineEdit *m_cityReplacement;
    ChangelogWidget *m_changelog;
    KActionCollection *m_actions;

    QSignalMapper *m_mapper;
};

#endif // Multiple inclusion guard
// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
