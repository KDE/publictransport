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

// Header
#include "changelogwidget.h"

// PublicTransport engine includes
#include "engine/accessorinfoxmlreader.h"

// KDE includes
#include <KLineEdit>
#include <KLocalizedString>
#include <KDebug>
#include <KIcon>

// Qt includes
#include <QFormLayout>
#include <QValidator>
#include <QToolButton>
#include <QMenu>

ChangelogEntryWidget::ChangelogEntryWidget( QWidget *parent, const ChangelogEntry &entry,
        const QString &shortAuthor ) : QWidget( parent )
{
    // Create layout
    QFormLayout *layout = new QFormLayout( this );

    QWidget *authorVersionWidget = new QWidget( this );
    QHBoxLayout *authorVersionLayout = new QHBoxLayout( authorVersionWidget );
    authorVersionLayout->setContentsMargins( 0, 0, 0, 0 );

    m_author = new KLineEdit( entry.author, authorVersionWidget );
    m_author->setClickMessage( shortAuthor );
    m_author->setFixedWidth( 125 );

    m_version = new KLineEdit( entry.since_version, authorVersionWidget );
    m_version->setFixedWidth( 75 );

    authorVersionLayout->addWidget( m_author );
    authorVersionLayout->addWidget( m_version );
    authorVersionLayout->addStretch();

    m_releasedWith = new KLineEdit( entry.released_with_version, this );
    m_releasedWith->setFixedWidth( 75 );

    m_description = new KLineEdit( entry.description, this );
    m_description->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );

    connect( m_author, SIGNAL(textChanged(QString)), this, SIGNAL(changed()) );
    connect( m_version, SIGNAL(textChanged(QString)), this, SIGNAL(changed()) );
    connect( m_releasedWith, SIGNAL(textChanged(QString)), this, SIGNAL(changed()) );
    connect( m_description, SIGNAL(textChanged(QString)), this, SIGNAL(changed()) );

    // Set a validator for version line edits
    QRegExpValidator *versionValidator = new QRegExpValidator( QRegExp( "\\d+(\\.\\d+)*" ), this );
    m_version->setValidator( versionValidator );
    m_releasedWith->setValidator( versionValidator );

    layout->addRow( i18nc("@info Label for the author of a changelog entry (short author name)",
                          "Author, Version:"), authorVersionWidget );
//  layout->addRow( i18nc("@info Label for the version of a changelog entry",
//                        "Version:"), m_version );
    m_releasedWith->hide(); // Not used, but read and save it
//  layout->addRow( i18nc("@info Label for the publictransport version the changelog entry was released with",
//                        "Released With:"), m_releasedWith );
    layout->addRow( i18nc("@info Label for the description of a changelog entry",
                          "Description:"), m_description );
}

ChangelogEntry ChangelogEntryWidget::changelogEntry() const
{
    ChangelogEntry entry;
    entry.author = m_author->text();
    entry.since_version = m_version->text();
    entry.released_with_version = m_releasedWith->text();
    entry.description = m_description->text();
    return entry;
}

QString ChangelogEntryWidget::author() const
{
    return m_author->text();
}

void ChangelogEntryWidget::setAuthor( const QString &author )
{
    m_author->setText( author );
}

QString ChangelogEntryWidget::version() const
{
    return m_version->text();
}

void ChangelogEntryWidget::setVersion( const QString &version )
{
    m_version->setText( version );
}

QString ChangelogEntryWidget::releasedWith() const
{
    return m_releasedWith->text();
}

void ChangelogEntryWidget::setReleasedWith( const QString &releasedWith )
{
    m_releasedWith->setText( releasedWith );
}

QString ChangelogEntryWidget::description() const
{
    return m_description->text();
}

void ChangelogEntryWidget::setDescription( const QString &description )
{
    m_description->setText( description );
}

void ChangelogEntryWidget::setChangelogEntry( const ChangelogEntry &changelogEntry,
        const QString &shortAuthor )
{
    setAuthor( changelogEntry.author );
    if( changelogEntry.author.isEmpty() ) {
        m_author->setClickMessage( shortAuthor );
    }
    setVersion( changelogEntry.since_version );
    setReleasedWith( changelogEntry.released_with_version );
    setDescription( changelogEntry.description );
}

ChangelogWidget::ChangelogWidget( QWidget *parent,
                                  AbstractDynamicWidgetContainer::RemoveButtonOptions removeButtonOptions,
                                  AbstractDynamicWidgetContainer::AddButtonOptions addButtonOptions,
                                  AbstractDynamicWidgetContainer::SeparatorOptions separatorOptions )
    : AbstractDynamicWidgetContainer( parent, removeButtonOptions, addButtonOptions,
                                      separatorOptions, AddWidgetsAtTop )
{
    QToolButton *btnAdd = addButton();
    btnAdd->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    btnAdd->setText( i18nc( "@action:button", "&Add Changelog Entry" ) );

    QMenu *addMenu = new QMenu( this );
    addMenu->addAction( KIcon( "list-add" ), i18nc( "@action:inmenu", "Add &Empty Changelog Entry" ),
                        this, SLOT( createAndAddWidget() ) );
    addMenu->addAction( KIcon( "list-add" ), i18nc( "@action:inmenu", "Add &Same Version Changelog Entry" ),
                        this, SLOT( createAndAddWidgetSameVersion() ) );
    addMenu->addAction( KIcon( "list-add" ), i18nc( "@action:inmenu", "Add &New Minor Version Changelog Entry" ),
                        this, SLOT( createAndAddWidgetNewMinorVersion() ) );
    addMenu->addAction( KIcon( "list-add" ), i18nc( "@action:inmenu", "Add New &Major Version Changelog Entry" ),
                        this, SLOT( createAndAddWidgetNewMajorVersion() ) );

    btnAdd->setPopupMode( QToolButton::MenuButtonPopup );
    btnAdd->setMenu( addMenu );
}

void ChangelogWidget::createAndAddWidgetSameVersion()
{
    if( widgetCount() == 0 ) {
        createAndAddWidget();
        return;
    }

    ChangelogEntryWidget *lastEntry = dynamicWidgets().last()->contentWidget<ChangelogEntryWidget *>();
    ChangelogEntryWidget *newEntry = qobject_cast< ChangelogEntryWidget * >( createNewWidget() );
    newEntry->setVersion( lastEntry->version() );
    addWidget( newEntry );
}

void ChangelogWidget::createAndAddWidgetNewMinorVersion()
{
    if( widgetCount() == 0 ) {
        createAndAddWidget();
        return;
    }

    ChangelogEntryWidget *lastEntry = dynamicWidgets().last()->contentWidget<ChangelogEntryWidget *>();
    ChangelogEntryWidget *newEntry = qobject_cast< ChangelogEntryWidget * >( createNewWidget() );
    QRegExp version( "(\\d+)\\.(\\d+)(\\.\\d+)*" );
    if( version.indexIn( lastEntry->version() ) != -1 ) {
        int major = version.cap( 1 ).toInt();
        int minor = version.cap( 2 ).toInt();
        QString rest = version.cap( 3 );
        newEntry->setVersion( QString( "%1.%2%3" ).arg( major ).arg( minor + 1 ).arg( rest ) );
    } else {
        newEntry->setVersion( lastEntry->version() );
    }
    addWidget( newEntry );
}

void ChangelogWidget::createAndAddWidgetNewMajorVersion()
{
    if( widgetCount() == 0 ) {
        createAndAddWidget();
        return;
    }

    ChangelogEntryWidget *lastEntry = dynamicWidgets().last()->contentWidget<ChangelogEntryWidget *>();
    ChangelogEntryWidget *newEntry = qobject_cast< ChangelogEntryWidget * >( createNewWidget() );
    QRegExp version( "(\\d+)\\.(\\d+)(\\.\\d+)*" );
    if( version.indexIn( lastEntry->version() ) != -1 ) {
        int major = version.cap( 1 ).toInt();
        int minor = 0; // Start with minor version number 0
        QString rest = version.cap( 3 );
        newEntry->setVersion( QString( "%1.%2%3" ).arg( major + 1 ).arg( minor ).arg( rest ) );
    } else {
        newEntry->setVersion( lastEntry->version() );
    }
    addWidget( newEntry );
}

QWidget *ChangelogWidget::createNewWidget()
{
    return new ChangelogEntryWidget( this );
}

DynamicWidget *ChangelogWidget::addWidget( QWidget *widget )
{
    QString clickMessage;
    if( !dynamicWidgets().isEmpty() ) {
        clickMessage = dynamicWidgets().first()->contentWidget<ChangelogEntryWidget *>()
                       ->authorLineEdit()->clickMessage();
    }
    DynamicWidget *dynamicWidget = AbstractDynamicWidgetContainer::addWidget( widget );
    KLineEdit *authorLineEdit = qobject_cast<ChangelogEntryWidget *>( widget )->authorLineEdit();
    authorLineEdit->setClickMessage( clickMessage );
    authorLineEdit->setFocus();
    return dynamicWidget;
}

void ChangelogWidget::clear()
{
    removeAllWidgets();
}

void ChangelogWidget::addChangelog( const QList< ChangelogEntry >& changelog,
                                    const QString &shortAuthor )
{
    for( int i = changelog.count() - 1; i >= 0; --i ) {
        addChangelogEntry( changelog[i], shortAuthor );
    }
}

void ChangelogWidget::addChangelogEntry( const ChangelogEntry &changelogEntry,
        const QString &shortAuthor )
{
    ChangelogEntryWidget *widget = qobject_cast<ChangelogEntryWidget *>( createNewWidget() );
    widget->setChangelogEntry( changelogEntry, shortAuthor );
    connect( widget, SIGNAL( changed() ), this, SIGNAL( changed() ) );
    addWidget( widget );
}

QList< ChangelogEntry > ChangelogWidget::changelog() const
{
    QList<ChangelogEntry> ret;
    QList<ChangelogEntryWidget *> entryWidgets = widgets<ChangelogEntryWidget *>();
    foreach( const ChangelogEntryWidget * entryWidget, entryWidgets ) {
        ret << entryWidget->changelogEntry();
    }
    return ret;
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
