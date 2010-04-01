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

#include "categorycombobox.h"

#include <KCategorizedView>
#include <KCategorizedSortFilterProxyModel>
#include <KCategoryDrawer>
#include <QApplication>
#include <QDesktopWidget>
#include <KDebug>
#include <kdeversion.h>

void CategoryComboBox::showPopup() {
    KComboBox::showPopup();

    KCategorizedView *catView = qobject_cast< KCategorizedView* >( view() );
    if ( !catView )
	return;

    KCategorizedSortFilterProxyModel *modelCategorized =
	dynamic_cast< KCategorizedSortFilterProxyModel* >( model() );
    if ( !modelCategorized || !modelCategorized->isCategorizedModel()
	    || view()->parentWidget()->height() > 200 )
	return;
    
    // Include categories in height calculation
    QStringList categories;
    for ( int row = 0; row < modelCategorized->rowCount(); ++row ) {
	categories << modelCategorized->data(
	modelCategorized->index(row, modelColumn(), catView->rootIndex()),
		KCategorizedSortFilterProxyModel::CategoryDisplayRole ).toString();
    }

    QStyleOption option;
    option.initFrom( this );
    int categoryHeight = catView->categoryDrawer()->categoryHeight(
	    modelCategorized->index(0, 0), option );

    categories.removeDuplicates();
    int categoryCount = categories.count();
    #if KDE_VERSION < KDE_MAKE_VERSION(4,4,0)
    int categoriesHeight = categoryCount * categoryHeight + (categoryCount - 1);
    #else
    int categoriesHeight = categoryCount * (categoryHeight + catView->categorySpacing())
			    - catView->categorySpacing();
    #endif
    QSize size = view()->parentWidget()->size();
    int resultHeight = size.height() + categoriesHeight + 20;
    
    QRect screen = QApplication::desktop()->screenGeometry(
		    QApplication::desktop()->screenNumber(this) );
    QPoint globalPos = view()->mapToGlobal( view()->pos() );
    resultHeight = qMin( resultHeight, screen.height() - globalPos.y() );

    size.setHeight( resultHeight );
    view()->parentWidget()->resize( size );
}
