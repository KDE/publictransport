
#include "categorycombobox.h"

#include <KCategorizedView>
#include <KCategorizedSortFilterProxyModel>
#include <KCategoryDrawer>
#include <QApplication>
#include <QDesktopWidget>
#include <KDebug>

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
    int categoriesHeight = categoryCount * categoryHeight +
			   (categoryCount - 1) * catView->categorySpacing();
    QSize size = view()->parentWidget()->size();
    int resultHeight = size.height() + categoriesHeight + 20;
    
    QRect screen = QApplication::desktop()->screenGeometry(
		    QApplication::desktop()->screenNumber(this) );
    QPoint globalPos = view()->mapToGlobal( view()->pos() );
    resultHeight = qMin( resultHeight, screen.height() - globalPos.y() );

    size.setHeight( resultHeight );
    view()->parentWidget()->resize( size );
}