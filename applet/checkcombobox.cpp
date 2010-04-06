/*
*   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#include "checkcombobox.h"

#include <QStylePainter>
#include <QAbstractItemView>
#include <qevent.h>

#include <KLocalizedString>
#include <KDebug>

class CheckComboboxPrivate {
    public:
	CheckComboboxPrivate() {
	    allowNoCheck = true;
	    multipleSelectionOptions = CheckCombobox::ShowIconList;
	    separator = ", ";
	    noSelectionText = i18nc( "@info/plain Default text of a CheckCombobox "
		    "if no item is checked", "(none)" );
	    allSelectedText = i18nc( "@info/plain Default text of a CheckCombobox "
		    "if all items are checked", "(all)" );
	};
	
	bool allowNoCheck;
	CheckCombobox::MultipleSelectionOptions multipleSelectionOptions;
	QString separator, noSelectionText, allSelectedText;
};

CheckCombobox::CheckCombobox( QWidget* parent ) : KComboBox( parent ),
		d_ptr(new CheckComboboxPrivate) {
    view()->setEditTriggers( QAbstractItemView::NoEditTriggers );
    view()->viewport()->installEventFilter( this );
}

CheckCombobox::~CheckCombobox() {
    delete d_ptr;
}

CheckCombobox::MultipleSelectionOptions CheckCombobox::multipleSelectionOptions() const {
    Q_D( const CheckCombobox );
    return d->multipleSelectionOptions;
}

void CheckCombobox::setMultipleSelectionOptions(
	CheckCombobox::MultipleSelectionOptions multipleSelectionOptions ) {
    Q_D( CheckCombobox );
    d->multipleSelectionOptions = multipleSelectionOptions;
    if ( multipleSelectionOptions == ShowStringList )
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    else
	setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
}

QString CheckCombobox::separator() const {
    Q_D( const CheckCombobox );
    return d->separator;
}

void CheckCombobox::setSeparator( const QString& separator ) {
    Q_D( CheckCombobox );
    d->separator = separator;
}

QString CheckCombobox::noSelectionText() const {
    Q_D( const CheckCombobox );
    return d->noSelectionText;
}

void CheckCombobox::setNoSelectionText( const QString& noSelectionText ) {
    Q_D( CheckCombobox );
    d->noSelectionText = noSelectionText;
}

QString CheckCombobox::allSelectedText() const {
    Q_D( const CheckCombobox );
    return d->allSelectedText;
}

void CheckCombobox::setAllSelectedText( const QString& allSelectedText ) {
    Q_D( CheckCombobox );
    d->allSelectedText = allSelectedText;
}

bool CheckCombobox::allowNoCheckedItem() const {
    Q_D( const CheckCombobox );
    return d->allowNoCheck;
}

void CheckCombobox::setAllowNoCheckedItem( bool allow ) {
    Q_D( CheckCombobox );
    d->allowNoCheck = allow;
}

void CheckCombobox::addItem( const QString& text ) {
    KComboBox::addItem( text );
    for ( int row = 0; row < model()->rowCount(); ++row ) {
	QModelIndex index = model()->index( row, modelColumn() );
	if ( !model()->data(index, Qt::CheckStateRole).isValid() )
	    model()->setData( index, Qt::Unchecked, Qt::CheckStateRole );
    }
}

void CheckCombobox::addItems( const QStringList& texts ) {
    KComboBox::addItems( texts );
    for ( int row = 0; row < model()->rowCount(); ++row ) {
	QModelIndex index = model()->index( row, modelColumn() );
	if ( !model()->data(index, Qt::CheckStateRole).isValid() )
	    model()->setData( index, Qt::Unchecked, Qt::CheckStateRole );
    }
}

void CheckCombobox::keyPressEvent( QKeyEvent* event ) {
    Q_D( CheckCombobox );
    KComboBox::keyPressEvent( event );
    if ( event->key() == Qt::Key_Space ) {
        // Don't let the last checked item get unchecked
        // if m_allowNoCheck is false
        bool wasChecked = view()->currentIndex().data(Qt::CheckStateRole) == Qt::Checked;
        if ( d->allowNoCheck || (!wasChecked || hasCheckedItems(2)) ) {
            view()->model()->setData( view()->currentIndex(),
                                      wasChecked ? Qt::Unchecked : Qt::Checked,
                                      Qt::CheckStateRole );
            emit checkedItemsChanged();
	    updateGeometry();
            update();
        }
    }
}

bool CheckCombobox::eventFilter( QObject* object, QEvent* event ) {
    Q_D( CheckCombobox );
    if ( object == view()->viewport() && event->type() == QEvent::MouseButtonRelease ) {
        QMouseEvent *mouseEvent = static_cast< QMouseEvent* >( event );
        if ( mouseEvent->button() == Qt::LeftButton ) {
            // Don't close the dropdown list, if an item was clicked.
            // Toggle the checked state instead.
            QModelIndex index = view()->indexAt( mouseEvent->pos() );
            if ( index.isValid() ) {
                // Don't let the last checked item get unchecked
                // if m_allowNoCheck is false
                bool wasChecked = index.data(Qt::CheckStateRole) == Qt::Checked;
                if ( d->allowNoCheck || (!wasChecked || hasCheckedItems(2)) ) {
                    view()->model()->setData( index,
                                              wasChecked ? Qt::Unchecked : Qt::Checked,
                                              Qt::CheckStateRole );
                    emit checkedItemsChanged();
		    updateGeometry();
                    update();
                }
                return true;
            }
        }
    }

    return KComboBox::eventFilter( object, event );
}

void CheckCombobox::paintEvent( QPaintEvent* ) {
    Q_D( const CheckCombobox );
    
    QStylePainter painter( this );
    painter.setPen( palette().color(QPalette::Text) );

    // Draw the combobox frame, focusrect and selected etc.
    QStyleOptionComboBox opt;
    initStyleOption( &opt );

    // Set text
    QModelIndexList items = checkedItems();
    QList< QIcon > icons;
    opt.currentText.clear();
    foreach ( const QModelIndex &index, items ) {
        if ( !opt.currentText.isEmpty() )
            opt.currentText += d->separator;
        opt.currentText += index.data().toString();
        icons << index.data( Qt::DecorationRole ).value<QIcon>();
    }

    painter.drawComplexControl( QStyle::CC_ComboBox, opt );

    // Draw the icon and text
    if ( icons.count() <= 1 || d->multipleSelectionOptions == ShowStringList ) {
        if ( icons.isEmpty() ) {
	    opt.currentText = d->noSelectionText;
            opt.currentIcon = QIcon();
	} else if ( icons.count() > 1 && icons.count() == view()->model()->rowCount() ) {
	    opt.currentText = d->allSelectedText;
        } else
            opt.currentIcon = icons.first();
        painter.drawControl( QStyle::CE_ComboBoxLabel, opt );
    } else {
        QRect rc = QRect( QPoint(0, 0), opt.iconSize );
        int shownIcons = 0;
        int width;
        do {
            ++shownIcons;
            width = (opt.iconSize.width() + 1) * shownIcons - 1;
            if ( shownIcons == icons.count() )
                break;

            // Leave some place for text, arrow, frame...
        } while ( width < opt.rect.width() - 80 );

        opt.iconSize.setWidth( width );
        QPixmap pix( opt.iconSize );
        pix.fill( Qt::transparent );
        QPainter p( &pix );
        for ( int i = 0; i < shownIcons; ++i ) {
            QIcon icon = icons[ i ];
            p.drawPixmap( rc, icon.pixmap(rc.size()) );

            int w = rc.width();
            rc.setRight( rc.right() + w + 1 );
            rc.setLeft( rc.left() + w + 1 );
        }
        p.end();
        opt.currentIcon = QIcon( pix );
	if ( icons.count() == view()->model()->rowCount() ) {
	    if ( shownIcons < icons.count() )
		opt.currentText = QChar(0x2026) + d->allSelectedText; // '...'
		else
		    opt.currentText = d->allSelectedText;
	} else {
	    if ( shownIcons < icons.count() ) {
		opt.currentText = QString( "%1%2/%3" ).arg( QChar(0x2026) ) // '...'
				.arg( icons.count() )
				.arg( view()->model()->rowCount() );
	    } else {
		opt.currentText = QString( "%1 / %2" ).arg( icons.count() )
				.arg( view()->model()->rowCount() );
	    }
	}
	
        painter.drawControl( QStyle::CE_ComboBoxLabel, opt );
    }
}

void CheckCombobox::setCheckedItems( const QModelIndexList& indices ) {
    QModelIndexList checked = checkedItems();
    foreach ( const QModelIndex &checkedIndex, checked )
	view()->model()->setData( checkedIndex, Qt::Unchecked, Qt::CheckStateRole );

    foreach ( const QModelIndex &index, indices )
	view()->model()->setData( index, Qt::Checked, Qt::CheckStateRole );

    updateGeometry();
    emit checkedItemsChanged();
}

void CheckCombobox::setCheckedRows( const QList< int > &rows ) {
    QModelIndexList indices;
    foreach ( int row, rows )
	indices << view()->model()->index( row, modelColumn() );
    setCheckedItems( indices );
}

QSize CheckCombobox::sizeHint() const {
    Q_D( const CheckCombobox );
    QSize size = KComboBox::sizeHint();
    
    int checkboxSpace = style()->pixelMetric( QStyle::PM_IndicatorWidth )
		      + style()->pixelMetric( QStyle::PM_CheckBoxLabelSpacing );
    QFontMetrics fm( font() );
    int minTextSpace = fm.width( "00 / 00" );
    QSize contentsSize( (iconSize().width() + 1) * checkedItems().count() + minTextSpace + 5,
			iconSize().height() );
    QStyleOptionComboBox opt;
    initStyleOption( &opt );
    if ( d->multipleSelectionOptions == ShowStringList ) {
	QModelIndexList items = checkedItems();
	if ( items.count() == count() )
	    opt.currentText = d->allSelectedText;
	else {
	    opt.currentText.clear();
	    foreach ( const QModelIndex &index, items ) {
		if ( !opt.currentText.isEmpty() )
		    opt.currentText += d->separator;
		opt.currentText += index.data().toString();
	    }
	}
    }
    QSize customSize = style()->sizeFromContents( QStyle::CT_ComboBox, &opt, contentsSize );
    
    size.setWidth( qMax(size.width() + checkboxSpace, customSize.width()) );
    return size;
}

void CheckCombobox::setItemCheckState( const QModelIndex& index,
				       Qt::CheckState checkState ) {
    Qt::CheckState prevCheckState = static_cast< Qt::CheckState >(
	    view()->model()->data( index, Qt::CheckStateRole ).toInt() );
    view()->model()->setData( index, checkState, Qt::CheckStateRole );
    updateGeometry();
    if ( prevCheckState != checkState )
	emit checkedItemsChanged();
}

bool CheckCombobox::hasCheckedItems( int count ) const {
    return view()->model()->match( view()->model()->index(0, 0),
                                   Qt::CheckStateRole, Qt::Checked,
                                   count, Qt::MatchExactly ).count() == count;
}

QModelIndexList CheckCombobox::checkedItems() const {
    return view()->model()->match( view()->model()->index(0, 0),
                                   Qt::CheckStateRole, Qt::Checked,
                                   -1, Qt::MatchExactly );
}

QList< int > CheckCombobox::checkedRows() const {
    QModelIndexList indices = checkedItems();
    QList< int > rows;
    foreach ( const QModelIndex &index, indices )
	rows << index.row();
    return rows;
}
