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
#include "journeysearchlistview.h"

// Own includes
#include "journeysearchmodel.h"

// KDE includes
#include <KLineEdit>
#include <KIconEffect>
#include <KMenu>
#include <KAction>
#include <KIcon>
#include <KGlobalSettings>
#include <KLocalizedString>
#include <KColorUtils>
#include <KDebug>

// Qt includes
#include <QHBoxLayout>
#include <QToolButton>
#include <QStyleOption>
#include <QPainter>
#include <QApplication>
#include <qevent.h>

JourneySearchListView::JourneySearchListView( QWidget *parent )
        : QListView(parent), m_removeJourneySearchAction(0)
{
    m_addJourneySearchAction = new KAction( KIcon("list-add"),
                                            i18nc("@action", "&Add Journey Search"), this );
    m_removeJourneySearchAction = new KAction( KIcon("list-remove"),
                                               i18nc("@action", "&Remove"), this );
    m_toggleFavoriteAction = new KAction( this );
    connect( m_addJourneySearchAction, SIGNAL(triggered()),
             this, SLOT(addJourneySearch()) );
    connect( m_removeJourneySearchAction, SIGNAL(triggered()),
             this, SLOT(removeCurrentJourneySearch()) );
    connect( m_toggleFavoriteAction, SIGNAL(triggered()),
             this, SLOT(toggleFavorite()) );
    addAction( m_addJourneySearchAction );
    addAction( m_removeJourneySearchAction );
    addAction( m_toggleFavoriteAction );

    setItemDelegate( new JourneySearchDelegate(this) );
}

void JourneySearchListView::contextMenuEvent( QContextMenuEvent *event )
{
    if ( !qobject_cast<JourneySearchModel*>(model()) ) {
        kDebug() << "Needs a JourneySearchModel!";
    }

    // Get the model index to create a context menu for
    QModelIndex index = indexAt( event->pos() );

    // Disable remove journey search action, if nothing is selected
    if ( m_removeJourneySearchAction ) {
        m_removeJourneySearchAction->setEnabled( index.isValid() );
    }

    // Update toggle favorite state action (add to/remove from favorites)
    if ( m_toggleFavoriteAction ) {
        bool isAddToFavoritesAction = false;
        if ( index.isValid() ) {
            m_toggleFavoriteAction->setEnabled( true );

            const bool isFavorite = index.data( JourneySearchModel::FavoriteRole ).toBool();
            if ( isFavorite ) {
                m_toggleFavoriteAction->setText( i18nc("@action", "Remove From Favorites") );
                m_toggleFavoriteAction->setIcon(
                        KIcon("favorites", 0, QStringList() << "list-remove") );
            } else {
                isAddToFavoritesAction = true;
            }
        } else {
            m_toggleFavoriteAction->setEnabled( false );

            // Ensure action is initialized
            isAddToFavoritesAction = true;
        }

        if ( isAddToFavoritesAction ) {
            m_toggleFavoriteAction->setText( i18nc("@action", "Add to Favorites") );
            m_toggleFavoriteAction->setIcon( KIcon("favorites", 0, QStringList() << "list-add") );
        }
    }

    KMenu::exec( actions(), event->globalPos() );
}

void JourneySearchListView::addJourneySearch()
{
    // Add icon and set as favorite
    JourneySearchModel *_model = qobject_cast<JourneySearchModel*>( model() );
    Q_ASSERT_X( _model, "JourneySearchListView::addJourneySearch()", "Needs a JourneySearchModel!" );
    JourneySearchModelItem *item = _model->addJourneySearch( QString(), QString(), true );
    QModelIndex index = _model->indexFromItem( item );

    // Start editing the new journey search
    setCurrentIndex( index );
    edit( index );
}

void JourneySearchListView::removeCurrentJourneySearch()
{
    QModelIndex index = currentIndex();
    if ( !index.isValid() ) {
        return;
    }

    // Remove the journey search item at the current index
    JourneySearchModel *_model = qobject_cast<JourneySearchModel*>( model() );
    Q_ASSERT_X( _model, "JourneySearchListView::addJourneySearch()", "Needs a JourneySearchModel!" );
    _model->removeJourneySearch( index );
}

void JourneySearchListView::toggleFavorite()
{
    QModelIndex index = currentIndex();
    if ( !index.isValid() ) {
        return;
    }

    // Toggle favorite state and resort the model
    JourneySearchModel *_model = qobject_cast<JourneySearchModel*>( model() );
    Q_ASSERT_X( _model, "JourneySearchListView::addJourneySearch()", "Needs a JourneySearchModel!" );
    JourneySearchModelItem *item = _model->item( index );
    item->setFavorite( !item->isFavorite() );
    _model->sort();
}

JourneySearchDelegate::JourneySearchDelegate( QObject *parent )
        : QStyledItemDelegate(parent)
{
}

QSize JourneySearchDelegate::sizeHint( const QStyleOptionViewItem &option,
                                       const QModelIndex &index ) const
{
    const int width = qMin( option.rect.width(), option.fontMetrics.width(index.data().toString()) );
    const int height = 2 * qMin( option.rect.height(), option.fontMetrics.height() + 2 );
    return QSize( width, height );
}

void JourneySearchDelegate::paint( QPainter *painter, const QStyleOptionViewItem &option,
                                   const QModelIndex &index ) const
{
    // Initialize style options
    QStyleOptionViewItemV4 opt = option;
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    initStyleOption( &opt, index );
    opt.icon = QIcon();
    opt.showDecorationSelected = true;

    // Draw background only
    opt.text = QString();
    style->drawControl( QStyle::CE_ItemViewItem, &opt, painter );

    // Draw text items and icon only while not in edit mode
    const bool isEditing = opt.state.testFlag( QStyle::State_Editing );
    if ( !isEditing ) {
        // Save painter state
        painter->save();

        // Calculate rectangles for the text items
        const QRect textRect = style->subElementRect( QStyle::SE_ItemViewItemText, &opt );
        const int vCenter = textRect.top() + textRect.height() / 2;
        const QRect nameRect( textRect.left(), vCenter - opt.fontMetrics.height(),
                              textRect.width(), opt.fontMetrics.height() );
        const QRect journeySearchRect( textRect.left(), vCenter,
                                       textRect.width(), opt.fontMetrics.height() );

        // Get text/background colors
        QColor textColor;
        QColor backgroundColor;
        if ( opt.state.testFlag(QStyle::State_Selected) ) {
            textColor = option.palette.color( QPalette::HighlightedText);
            backgroundColor = option.palette.color( QPalette::Highlight );
        } else {
            textColor = option.palette.color( QPalette::Text );
            backgroundColor = option.palette.color( QPalette::Background );
        }

        // Get strings for the text items and a lighter color for the journey search string.
        // The journey search string color mixes the text color with the background color.
        const QString name = index.data(JourneySearchModel::NameRole).toString();
        const QString journeySearch = index.data(JourneySearchModel::JourneySearchRole).toString();
        const QColor lightColor = KColorUtils::mix( textColor, backgroundColor, 0.4 );

        // Draw the text items
        if ( name.isEmpty() ) {
            // No name specified for the journey search
            painter->setPen( textColor );
            painter->drawText( nameRect, i18nc("@info/plain", "(No name specified)") );

            // Draw journey search string in lighter color
            painter->setPen( lightColor );
            painter->drawText( journeySearchRect, journeySearch );
        } else {
            // A name is specified, draw it in bold font
            QFont boldFont = opt.font;
            boldFont.setBold( true );
            painter->setFont( boldFont);
            painter->setPen( textColor );
            painter->drawText( nameRect, name );

            // Draw journey search string in lighter color and with default font
            painter->setFont( opt.font );
            painter->setPen( lightColor );
            painter->drawText( journeySearchRect, journeySearch );
        }

        // Draw icon
        const bool isFavorite = index.data( JourneySearchModel::FavoriteRole ).toBool();
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        const QRect iconRect = style->subElementRect( QStyle::SE_ItemViewItemDecoration, &opt );
        style->drawItemPixmap( painter, iconRect, opt.decorationAlignment,
                icon.pixmap(opt.decorationSize, isFavorite ? QIcon::Normal : QIcon::Disabled,
                            opt.state.testFlag(QStyle::State_MouseOver) ? QIcon::On : QIcon::Off) );

        // Restore painter state
        painter->restore();
    }
}

QWidget* JourneySearchDelegate::createEditor( QWidget *parent, const QStyleOptionViewItem &option,
                                              const QModelIndex &index ) const
{
    // Create container editor widget
    QWidget *widget = new QWidget( parent );

    // Get rectangles
    QStyleOptionViewItemV4 opt = option;
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    initStyleOption( &opt, index );
    const QRect iconRect = style->subElementRect( QStyle::SE_ItemViewItemDecoration, &opt );
    const QRect textRect = style->subElementRect( QStyle::SE_ItemViewItemText, &opt );

    // Create favorite toggle button
    ToggleIconToolButton *button = new ToggleIconToolButton( widget );
    button->setIcon( index.data(Qt::DecorationRole).value<QIcon>() );
    button->setToolButtonStyle( Qt::ToolButtonIconOnly );
    button->setFixedSize( iconRect.size() );
    button->setAutoRaise( true );
    button->setCheckable( true );
    button->setToolTip( i18nc("@info:tooltip", "Toggle favorite state") );

    // Create name edit widget
    KLineEdit *lineEditName = new KLineEdit( widget );
    lineEditName->setText( index.data(JourneySearchModel::NameRole).toString() );
    lineEditName->setFrame( false );
    lineEditName->setClickMessage(
            i18nc("@info/plain Click message for the widget editing the journey search name.",
                  "Name of the journey search") );
    lineEditName->setToolTip(
            i18nc("@info:tooltip", "The name for the journey search string, eg. shown in menus.") );

    // Create journey search edit widget
    KLineEdit *lineEditJourneySearch = new KLineEdit( widget );
    lineEditJourneySearch->setText( index.data(JourneySearchModel::JourneySearchRole).toString() );
    lineEditJourneySearch->setFrame( false );
    lineEditJourneySearch->setClickMessage(
            i18nc("@info/plain Click message for the widget editing the journey search string.",
                  "Journey search string") );
    lineEditJourneySearch->setToolTip(
            i18nc("@info:tooltip", "This string gets used to request journeys.") );

    // Create layouts for the three widgets
    QVBoxLayout *vLayout = new QVBoxLayout();
    vLayout->setMargin( 0 );
    vLayout->setSpacing( 0 );
    vLayout->addWidget( lineEditName );
    vLayout->addWidget( lineEditJourneySearch );

    QHBoxLayout *layout = new QHBoxLayout( widget );
    layout->setContentsMargins( iconRect.left(), 0, 0, 0 );
    layout->setSpacing( textRect.left() - iconRect.right() );
    layout->addWidget( button );
    layout->addLayout( vLayout );

    // Initialize editor widget
    setEditorData( widget, index );

    // Enable focus for the editor widget,
    // otherwise editing may be cancelled when clicking a subwidget
    widget->setFocusPolicy( Qt::StrongFocus );

    // Use the line edit as focus proxy for the container widget,
    // ie. set focus to the line edit when the container widget gets focus,
    // which is the editor widget
    widget->setFocusProxy( lineEditName );

    // Set the focus to the line edit and select all text in it
    lineEditName->selectAll();
    lineEditName->setFocus();

    return widget;
}

void JourneySearchDelegate::setEditorData( QWidget *editor, const QModelIndex &index ) const
{
    if ( editor->layout()->count() < 2 ) {
        return; // TODO
    }
    QToolButton *button = qobject_cast<QToolButton*>( editor->layout()->itemAt(0)->widget() );
    QLayout *vLayout = editor->layout()->itemAt(1)->layout();
    KLineEdit *lineEditName = qobject_cast<KLineEdit*>( vLayout->itemAt(0)->widget() );
    KLineEdit *lineEditJourneySearch = qobject_cast<KLineEdit*>( vLayout->itemAt(1)->widget() );
    if ( button && lineEditName && lineEditJourneySearch ) {
        // Update widget states
        const bool isFavorite = index.data( JourneySearchModel::FavoriteRole ).toBool();
        button->setChecked( isFavorite );
        lineEditName->setText( index.data(JourneySearchModel::NameRole).toString() );
        lineEditJourneySearch->setText( index.data(JourneySearchModel::JourneySearchRole).toString() );
    } else {
        QStyledItemDelegate::setEditorData( editor, index );
    }
}

void JourneySearchDelegate::setModelData( QWidget *editor, QAbstractItemModel *model,
                                          const QModelIndex &index ) const
{
    if ( editor->layout()->count() < 2 ) {
        return; // TODO
    }
    QToolButton *button = qobject_cast<QToolButton*>( editor->layout()->itemAt(0)->widget() );
    QLayout *vLayout = editor->layout()->itemAt(1)->layout();
    KLineEdit *lineEditName = qobject_cast<KLineEdit*>( vLayout->itemAt(0)->widget() );
    KLineEdit *lineEditJourneySearch = qobject_cast<KLineEdit*>( vLayout->itemAt(1)->widget() );
    if ( button && lineEditName && lineEditJourneySearch ) {
        if ( lineEditJourneySearch->text().isEmpty() ) {
            // Remove items with empty journey search string
            model->removeRow( index.row() );
            return;
        }

        // Update both journey search string and favorite state at once in the model
        QMap<int, QVariant> roles;
        roles.insert( JourneySearchModel::NameRole, lineEditName->text() );
        roles.insert( JourneySearchModel::JourneySearchRole, lineEditJourneySearch->text() );
        roles.insert( JourneySearchModel::FavoriteRole, button->isChecked() );
        model->setItemData( index, roles );
        model->sort( 0 );
    } else {
        QStyledItemDelegate::setModelData( editor, model, index );
    }
}

void JourneySearchDelegate::updateEditorGeometry( QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
    Q_UNUSED( index );
    editor->setGeometry( option.rect );
}

void ToggleIconToolButton::paintEvent( QPaintEvent *event )
{
    Q_UNUSED( event );
    QPainter painter( this );
    QPixmap _icon = JourneySearchModel::favoriteIconPixmap( isChecked() );
    if ( underMouse() ) {
        // Draw a highlighted version of the icon if the button is hovered
        QPixmap highlightIcon = KIconLoader::global()->iconEffect()->apply(
                _icon, KIconEffect::ToGamma, 1.0, QColor(), false );
        painter.drawPixmap( contentsRect(), highlightIcon );
    } else {
        // Draw default icon
        painter.drawPixmap( contentsRect(), _icon );
    }
}
