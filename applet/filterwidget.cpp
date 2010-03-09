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

#include "filterwidget.h"

#include "filter.h"
#include "departureinfo.h"

#include <QLabel>


FilterWidget::FilterWidget( QWidget* parent )
	    : AbstractDynamicLabeledWidgetContainer( RemoveButtonsBesideWidgets,
	      AddButtonBesideFirstWidget, NoSeparator, QString(), parent ) {
    setWidgetCountRange( 1, 10, false );
    setAutoRaiseButtons( true );
    setRemoveButtonIcon( "edit-delete" );
}

FilterWidget::FilterWidget( QList< FilterType > filterTypes, QWidget* parent )
	    : AbstractDynamicLabeledWidgetContainer( RemoveButtonsBesideWidgets,
	      AddButtonBesideFirstWidget, NoSeparator, QString(), parent ) {
    setWidgetCountRange( 1, 10, false );
    setAutoRaiseButtons( true );
    setRemoveButtonIcon( "edit-delete" );
    foreach ( FilterType filterType, filterTypes )
	addConstraint( filterType );
}

FilterType FilterWidget::firstUnusedFilterType() const {
    QList< FilterType > filterTypes;
    foreach ( ConstraintWidget* constraint, constraintWidgets() )
	filterTypes << constraint->type();

    QList< FilterType > allFilterTypes;
    allFilterTypes << FilterByVehicleType << FilterByTarget
	    << FilterByTransportLine << FilterByTransportLineNumber << FilterByDelay;
    foreach ( FilterType filterType, allFilterTypes ) {
	if ( !filterTypes.contains(filterType) )
	    return filterType;
    }

    return FilterByTarget; // Already used
}

void FilterWidget::addConstraint( ConstraintWidget* filter ) {
    KComboBox *cmbFilterType = qobject_cast< KComboBox* >( createNewLabelWidget(0) );
    Q_ASSERT( cmbFilterType );
    DynamicWidget *dynamicWidget = addWidget( cmbFilterType, filter );
    if ( !dynamicWidget )
	kDebug() << "COULDN'T ADD CONSTRAINT";
}

void FilterWidget::removeConstraint( ConstraintWidget *widget ) {
    removeWidget( widget );
}

int FilterWidget::removeWidget( QWidget* widget ) {
    ConstraintWidget *filter = qobject_cast< ConstraintWidget* >( widget );
    Constraint removedConstraint;
    if ( filter )
	removedConstraint = filter->constraint();
	
    int index = AbstractDynamicLabeledWidgetContainer::removeWidget( widget );
    if ( index != -1 ) {
	m_filterTypes.removeAt( index );
	emit changed();
	if ( filter )
	    emit constraintRemoved( removedConstraint );
    }
    return index;
}

ConstraintWidget* ConstraintWidget::create( FilterType type, FilterVariant variant,
					    const QVariant &value, QWidget *parent ) {
    QList< VehicleType > filterVehicleTypes;
    QList< ConstraintListWidget::ListItem > values;
    switch ( type ) {
    case FilterByVehicleType:
	filterVehicleTypes = QList< VehicleType >()
		<< Unknown << Tram << Bus << TrolleyBus << Subway << TrainInterurban
		<< Metro << TrainRegional << TrainRegionalExpress << TrainInterregio
		<< TrainIntercityEurocity << TrainIntercityExpress << Ferry << Plane;
        foreach ( VehicleType vehicleType, filterVehicleTypes ) {
	    values << ConstraintListWidget::ListItem(
		    Global::vehicleTypeToString(vehicleType),
		    static_cast<int>(vehicleType),
		    Global::iconFromVehicleType(vehicleType) );
        }
        return new ConstraintListWidget( type, variant, values, value.toList(), parent );

    case FilterByTransportLine:
    case FilterByTarget:
	return new ConstraintStringWidget( type, variant, value.toString(), parent );

    case FilterByTransportLineNumber:
    case FilterByDelay:
	return new ConstraintIntWidget( type, variant, value.toInt(), 0, 10000, parent );

    default:
        kDebug() << "Unknown filter type" << type;
        return NULL;
    }
}

ConstraintWidget* FilterWidget::createFilter( FilterType type ) {
    switch ( type ) {
    case FilterByVehicleType:
	return ConstraintWidget::create( type, FilterIsOneOf, QVariantList() << Unknown, this );

    case FilterByTransportLine:
    case FilterByTarget:
	return ConstraintWidget::create( type, FilterContains, QString(), this );

    case FilterByTransportLineNumber:
    case FilterByDelay:
	return ConstraintWidget::create( type, FilterEquals, 0, this );
// 	return new FilterIntWidget( type, FilterEquals, 0, 0, 10000, this );

    default:
        kDebug() << "Unknown filter type" << type;
        return NULL;
    }
}

QString FilterWidget::filterName( FilterType filter ) const {
    switch ( filter ) {
    case FilterByVehicleType:
        return i18n("Vehicle");
    case FilterByTransportLine:
        return i18n("Line string");
    case FilterByTransportLineNumber:
	return i18n("Line number");
    case FilterByTarget:
        return i18n("Target");
    case FilterByDelay:
	return i18n("Delay");

    default:
        kDebug() << "Filter unknown" << filter;
        return QString();
    }
}

void FilterWidget::filterTypeChanged( int index ) {
    if( index < 0 ) {
	kDebug() << "No new index (-1)";
	return;
    }
kDebug() << index << "D'OH!";

    KComboBox *cmbFilterType = qobject_cast< KComboBox* >( sender() );
    if ( !cmbFilterType ) // sender is this, called from addConstraint()...
        cmbFilterType = m_filterTypes.last(); // ...therefore the combo box is the last in the list
    Q_ASSERT( cmbFilterType );
    int filterIndex = m_filterTypes.indexOf( cmbFilterType );
    kDebug() << "FilterIndex =" << filterIndex << dynamicWidgets().count();
//     QFormLayout *l = dynamic_cast< QFormLayout* >( layout() );
//     Q_ASSERT( l );

    FilterType type = static_cast< FilterType >( cmbFilterType->itemData(index).toInt() );
//     ConstraintWidget *oldFilter = m_constraintWidgets[ filterIndex ];
//     QWidgetList permanentWidgets;
//     int row;
//     if ( oldFilter ) {
//         QFormLayout::ItemRole role;
// 	l->getWidgetPosition( oldFilter, &row, &role );
// 	l->removeWidget( oldFilter );
// 	permanentWidgets = oldFilter->permanentWidgets();
// 	oldFilter->removePermanentWidgets();
//     } else
// 	row = l->rowCount() - 1;

    ConstraintWidget *newFilter = createFilter( type );
    dynamicWidgets().at( filterIndex )->replaceContentWidget( newFilter );
//     if ( newFilter ) {
// 	l->setWidget( row, QFormLayout::FieldRole, newFilter );
// 	newFilter->addPermanentWidgets( permanentWidgets );
// 	if ( filterIndex > 0 || !m_closeButton )
// 	    newFilter->layout()->addItem( new QSpacerItem(closeButtonSpacing(), 0) );
//     }

//     if ( oldFilter )
// 	delete oldFilter;
    
//     m_constraintWidgets[ filterIndex ] = newFilter;
    connect( newFilter, SIGNAL(changed()), this, SIGNAL(changed()) );
    emit changed();
}

QString ConstraintWidget::filterVariantName( FilterVariant filterVariant ) const {
    switch ( filterVariant ) {
    case FilterContains:
        return i18n("Contains");
    case FilterDoesntContain:
        return i18n("Does not Contain");
    case FilterEquals:
        return i18n("Equals");
    case FilterDoesntEqual:
        return i18n("Does not Equal");
    case FilterMatchesRegExp:
        return i18n("Matches Regular Expr.");
    case FilterDoesntMatchRegExp:
        return i18n("Doesn't Match Reg. Expr.");
	
    case FilterIsOneOf:
	return i18n("One of");
    case FilterIsntOneOf:
	return i18n("None of");

    case FilterGreaterThan:
	return i18n("Greater Than");
    case FilterLessThan:
	return i18n("Less Than");

    default:
        kDebug() << "Filter variant unknown" << filterVariant;
        return QString();
    }
}

ConstraintWidget::ConstraintWidget( FilterType type, QList< FilterVariant > availableVariants,
			    FilterVariant initialVariant, QWidget* parent )
			    : QWidget( parent ), m_containerWidget(0) {
    m_constraint.type = type;
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
    QHBoxLayout *layout = new QHBoxLayout( this );
    setLayout( layout );
    layout->setContentsMargins( 0, 0, 0, 0 );

    if ( !availableVariants.isEmpty() ) {
	if ( !availableVariants.contains(initialVariant) ) {
	    kDebug() << "Initial variant" << initialVariant << "not found in" << availableVariants;
	    initialVariant = availableVariants.first();
	}
	
        m_variantsCmb = new KComboBox( this );
	m_variantsCmb->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
        foreach ( FilterVariant variant, availableVariants ) {
            m_variantsCmb->addItem( filterVariantName(variant),
                                    static_cast<int>(variant) );
        }
        int index = m_variantsCmb->findData( static_cast<int>(initialVariant) );
	m_variantsCmb->setCurrentIndex( index );
        variantChanged( index );
        connect( m_variantsCmb, SIGNAL(currentIndexChanged(int)),
                 this, SLOT(variantChanged(int)) );
        layout->addWidget( m_variantsCmb );
    } else
	m_constraint.variant = FilterNoVariant;
}

ConstraintListWidget::ConstraintListWidget( FilterType type, FilterVariant initialVariant,
				    const QList< ListItem > &values,
				    const QVariantList& initialValues, QWidget* parent )
				    : ConstraintWidget( type, QList< FilterVariant >()
				      << FilterIsOneOf << FilterIsntOneOf,
				      initialVariant, parent ) {
    m_list = new CheckCombobox( this );
    QStandardItemModel *model = new QStandardItemModel( this );
    foreach ( const ListItem &listItem, values ) {
	QStandardItem *item = new QStandardItem( listItem.icon, listItem.text );
	item->setData( listItem.value, Qt::UserRole );
        item->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable );
        item->setData( Qt::Unchecked, Qt::CheckStateRole );
        model->appendRow( item );
    }
    m_list->setModel( model );
    m_list->setAllowNoCheckedItem( false );
    addWidget( m_list );

    setValue( initialValues );
    checkedItemsChanged();

    connect( m_list, SIGNAL(checkedItemsChanged()),
             this, SLOT(checkedItemsChanged()) );
// 	    connect( m_list, SIGNAL(currentIndexChanged(int)),
// 		     this, SLOT(valueChanged(int)) );
}

ConstraintStringWidget::ConstraintStringWidget( FilterType type, FilterVariant initialVariant,
					const QString& filterText, QWidget* parent )
					: ConstraintWidget( type, QList< FilterVariant >()
					  << FilterContains << FilterDoesntContain
					  << FilterEquals << FilterDoesntEqual
					  << FilterMatchesRegExp << FilterDoesntMatchRegExp,
					  initialVariant, parent ) {
    m_string = new KLineEdit( this );
    m_string->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    m_string->setMinimumWidth( 50 );
    m_string->setClearButtonShown( true );
    m_string->setText( filterText );
    addWidget( m_string );

    connect( m_string, SIGNAL(textChanged(QString)), this, SLOT(stringChanged(QString)) );
}

ConstraintIntWidget::ConstraintIntWidget( FilterType type, FilterVariant initialVariant,
				  int value, int min, int max, QWidget* parent )
				  : ConstraintWidget( type, QList< FilterVariant >()
				    << FilterEquals << FilterDoesntEqual
				    << FilterGreaterThan << FilterLessThan,
				    initialVariant, parent ) {
    m_num = new KIntSpinBox( this );
    m_num->setRange( min, max );
    m_num->setValue( value );
    addWidget( m_num );

    connect( m_num, SIGNAL(valueChanged(int)), this, SLOT(intChanged(int)) );
}

void ConstraintListWidget::setValue( const QVariant& value ) {
    QModelIndexList indices/* = m_list->checkedItems()*/;
    if ( value.isValid() ) {
	QVariantList values = value.toList();
	foreach ( QVariant value, values ) {
	    QModelIndex index = indexFromValue( value );
	    if ( index.isValid() )
		indices << index;
	    else
		kDebug() << "Value" << value << "not found";
	}
    }

    m_list->setCheckedItems( indices );
}

QModelIndex ConstraintListWidget::indexFromValue( const QVariant& value ) {
    QModelIndexList indices = m_list->model()->match(
	    m_list->model()->index(0, 0), Qt::UserRole, value, 1, Qt::MatchExactly );
    
    return indices.isEmpty() ? QModelIndex() : indices.first();
}

FilterListWidget::FilterListWidget( QWidget* parent ) : AbstractDynamicWidgetContainer(
			    RemoveButtonsBesideWidgets, AddButtonAfterLastWidget,
			    ShowSeparators, parent ) {
    setWidgetCountRange( 1, 10, false );
    addButton()->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    addButton()->setText( i18n("&Add Filter") );
    addButton()->setToolTip( i18n("Add another filter") );
}

