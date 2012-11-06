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
#include "filterwidget.h"

// Own includes
#include "checkcombobox.h"

// KDE includes
#include <KGlobal>
#include <KLocale>
#include <KLineEdit>
#include <KNumInput>
#include <KDateComboBox>

// Qt includes
#include <QStandardItemModel>
#include <QFormLayout>
#include <QTimeEdit>
#include <QToolButton>

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

FilterWidget::FilterWidget( QWidget* parent,
                            AbstractDynamicWidgetContainer::SeparatorOptions seperatorOptions )
        : AbstractDynamicLabeledWidgetContainer( parent, RemoveButtonsBesideWidgets,
        AddButtonBesideFirstWidget, seperatorOptions, AddWidgetsAtBottom, QString() )
{
    m_allowedFilterTypes << FilterByVehicleType << FilterByTarget << FilterByVia << FilterByNextStop
            << FilterByTransportLine << FilterByTransportLineNumber << FilterByDelay;
    setWidgetCountRange( 1, 10, false );
    setAutoRaiseButtons( true );
    setRemoveButtonIcon( "edit-delete" );
}

FilterWidget::FilterWidget( const QList<FilterType> &allowedFilterTypes, QWidget* parent,
                            AbstractDynamicWidgetContainer::SeparatorOptions seperatorOptions )
        : AbstractDynamicLabeledWidgetContainer( parent, RemoveButtonsBesideWidgets,
        AddButtonBesideFirstWidget, seperatorOptions, AddWidgetsAtBottom, QString() )
{
    if ( allowedFilterTypes.isEmpty() ) {
        // Add default allowed filter types
        m_allowedFilterTypes << FilterByVehicleType << FilterByTarget << FilterByVia
                << FilterByNextStop << FilterByTransportLine << FilterByTransportLineNumber
                << FilterByDelay;
    } else {
        m_allowedFilterTypes = allowedFilterTypes;
    }
    setWidgetCountRange( 1, 10, false );
    setAutoRaiseButtons( true );
    setRemoveButtonIcon( "edit-delete" );
}

void FilterWidget::setAllowedFilterTypes( const QList< FilterType >& allowedFilterTypes )
{
    m_allowedFilterTypes = allowedFilterTypes;
}

FilterType FilterWidget::firstUnusedFilterType() const
{
    // Collect used filter types
    QList< FilterType > filterTypes;
    foreach( ConstraintWidget* constraint, constraintWidgets() ) {
        filterTypes << constraint->type();
    }

    // Go through all allowed filter types and return the first which is not used
    foreach( FilterType filterType, m_allowedFilterTypes ) {
        if ( !filterTypes.contains(filterType) ) {
            // filterType is allowed but not used, return it
            return filterType;
        }
    }

    return FilterByTarget; // Already used
}

void FilterWidget::setFilter( const Filter& filter )
{
    if ( !dynamicWidgets().isEmpty() ) {
        // Clear old filter widgets
        int minWidgetCount = minimumWidgetCount();
        int maxWidgetCount = maximumWidgetCount();
        setWidgetCountRange();
        removeAllWidgets();

        // Setup ConstraintWidgets from filters
        foreach( const Constraint &constraint, filter ) {
            addConstraint( constraint );
        }

        // Restor widget count range
        setWidgetCountRange( minWidgetCount, maxWidgetCount );
    } else {
        foreach( const Constraint &constraint, filter ) {
            addConstraint( constraint );
        }
    }
}

void FilterWidget::addConstraint( ConstraintWidget* filter )
{
    // Add a combobox as label for the given filter
    KComboBox *cmbFilterType = qobject_cast< KComboBox* >( createNewLabelWidget(0) );
    Q_ASSERT( cmbFilterType );

    // Add the given filter in a DynamicWidget
    DynamicWidget *dynamicWidget = addWidget( cmbFilterType, filter );
    if ( !dynamicWidget ) {
        kDebug() << "Couldn't add constraint widget" << filter;
    }
}

void FilterWidget::removeConstraint( ConstraintWidget *widget )
{
    removeWidget( widget );
}

int FilterWidget::removeWidget( QWidget* widget )
{
    ConstraintWidget *filter = qobject_cast< ConstraintWidget* >( widget );
    Constraint removedConstraint;
    if ( filter ) {
        removedConstraint = filter->constraint();
    }

    int index = AbstractDynamicLabeledWidgetContainer::removeWidget( widget );
    if ( index != -1 ) {
        m_filterTypes.removeAt( index );
        emit changed();
        if ( filter ) {
            emit constraintRemoved( removedConstraint );
        }
    }
    return index;
}

ConstraintWidget* ConstraintWidget::create( FilterType type, FilterVariant variant,
                                            const QVariant &value, QWidget *parent )
{
    switch ( type ) {
    case FilterByVehicleType: {
        QList< VehicleType > filterVehicleTypes;
        QList< ConstraintListWidget::ListItem > values;
        filterVehicleTypes = QList< VehicleType >()
                << UnknownVehicleType << Tram << Bus << TrolleyBus << Subway << InterurbanTrain
                << Metro << RegionalTrain << RegionalExpressTrain << InterregionalTrain
                << IntercityTrain << HighSpeedTrain << Ferry << Plane;
        foreach( VehicleType vehicleType, filterVehicleTypes ) {
            values << ConstraintListWidget::ListItem(
                    Global::vehicleTypeToString(vehicleType), static_cast<int>(vehicleType),
                    Global::vehicleTypeToIcon(vehicleType) );
        }
        return new ConstraintListWidget( type, variant, values, value.toList(), parent );
    }

    case FilterByTransportLine:
    case FilterByTarget:
    case FilterByVia:
    case FilterByNextStop:
        return new ConstraintStringWidget( type, variant, value.toString(), parent );

    case FilterByTransportLineNumber:
    case FilterByDelay:
        return new ConstraintIntWidget( type, variant, value.toInt(), 0, 10000, parent );

    case FilterByDepartureTime:
        return new ConstraintTimeWidget( type, variant, value.toTime(), parent );
    case FilterByDepartureDate:
        return new ConstraintDateWidget( type, variant, value.toDate(), parent );

    case FilterByDayOfWeek: {
        QList< int > filterDaysOfWeek;
        QList< ConstraintListWidget::ListItem > values;
        int weekStartDay = KGlobal::locale()->weekStartDay();
        for ( int weekday = weekStartDay; weekday <= 7; ++weekday ) {
            filterDaysOfWeek << weekday;
            if (( weekStartDay > 1 && weekday == weekStartDay - 1 )
                    || ( weekStartDay == 1 && weekday == 7 ) ) {
                break;
            } else if ( weekday == 7 ) {
                weekday = 1;
            }
        }

        foreach( int dayOfWeek, filterDaysOfWeek ) {
            values << ConstraintListWidget::ListItem(
                    QDate::longDayName(dayOfWeek, QDate::StandaloneFormat), dayOfWeek, KIcon() );
        }
        ConstraintListWidget *listWidget = new ConstraintListWidget( type,
                variant, values, value.toList(), parent );
        listWidget->list()->setAllSelectedText( i18nc( "@info/plain Text of a "
                "CheckCombobox with weekday names if all days are checked", "(all days)" ) );
        listWidget->list()->setMultipleSelectionOptions( CheckCombobox::ShowStringList );
        return listWidget;
    }

    default:
        kDebug() << "Unknown filter type" << type;
        return 0;
    }
}

ConstraintWidget* FilterWidget::createConstraint( FilterType type )
{
    switch ( type ) {
    case FilterByVehicleType:
        return ConstraintWidget::create( type, FilterIsOneOf, QVariantList() << UnknownVehicleType, this );

    case FilterByTransportLine:
    case FilterByTarget:
    case FilterByVia:
    case FilterByNextStop:
        return ConstraintWidget::create( type, FilterContains, QString(), this );

    case FilterByTransportLineNumber:
    case FilterByDelay:
        return ConstraintWidget::create( type, FilterEquals, 0, this );

    case FilterByDepartureTime:
        return ConstraintWidget::create( type, FilterEquals, QTime::currentTime(), this );
    case FilterByDepartureDate:
        return ConstraintWidget::create( type, FilterEquals, QDate::currentDate(), this );
    case FilterByDayOfWeek:
        return ConstraintWidget::create( type, FilterIsOneOf,
                                         QVariantList() << 1 << 2 << 3 << 4 << 5 << 6 << 7, this );

    default:
        kDebug() << "Unknown filter type" << type;
        return 0;
    }
}

QString FilterWidget::filterName( FilterType filter ) const
{
    switch ( filter ) {
    case FilterByVehicleType:
        return i18nc( "@item:inlistbox Name of the filter for vehicle types", "Vehicle" );
    case FilterByTransportLine:
        return i18nc( "@item:inlistbox Name of the filter for transport line strings", "Line string" );
    case FilterByTransportLineNumber:
        return i18nc( "@item:inlistbox Name of the filter for transport line numers, "
                      "eg. 6 when the transport line string is 'N6'", "Line number" );
    case FilterByTarget:
        return i18nc( "@item:inlistbox Name of the filter for targets/origins", "Target" );
    case FilterByVia:
        return i18nc( "@item:inlistbox Name of the filter for intermediate stops", "Via" );
    case FilterByNextStop:
        return i18nc( "@item:inlistbox Name of the filter for the first intermediate stop", "Next Stop" );
    case FilterByDelay:
        return i18nc( "@item:inlistbox Name of the filter for delays", "Delay" );
    case FilterByDepartureTime:
        return i18nc( "@item:inlistbox Name of the filter for departure times", "Departure Time" );
    case FilterByDepartureDate:
        return i18nc( "@item:inlistbox Name of the filter for departure dates", "Departure Date" );
    case FilterByDayOfWeek:
        return i18nc( "@item:inlistbox Name of the filter for departure weekdays", "Day of Week" );

    default:
        kDebug() << "Filter unknown" << filter;
        return QString();
    }
}

void FilterWidget::filterTypeChanged( int index )
{
    if ( index < 0 ) {
        kDebug() << "No new index (-1)";
        return;
    }

    KComboBox *cmbFilterType = qobject_cast< KComboBox* >( sender() );
    if ( !cmbFilterType ) { // sender is this, called from addConstraint()...
        cmbFilterType = m_filterTypes.last(); // ...therefore the combo box is the last in the list
    }
    Q_ASSERT( cmbFilterType );
    int filterIndex = m_filterTypes.indexOf( cmbFilterType );

    // Get the new filter type
    FilterType type = static_cast< FilterType >( cmbFilterType->itemData( index ).toInt() );

    // Create a ConstraintWidget for the new filter type and replace the old ConstraintWidget
    ConstraintWidget *newConstraint = createConstraint( type );
    dynamicWidgets().at( filterIndex )->replaceContentWidget( newConstraint );
    connect( newConstraint, SIGNAL(changed()), this, SIGNAL(changed()) );
    emit changed();
}

QString ConstraintWidget::filterVariantName( FilterVariant filterVariant ) const
{
    switch ( filterVariant ) {
    case FilterContains:
        return i18nc( "@item:inlistbox Name of the filter variant that matches "
                      "the filter word is contained", "Contains" );
    case FilterDoesntContain:
        return i18nc( "@item:inlistbox Name of the filter variant that matches if "
                      "the filter word is not contained", "Does not Contain" );
    case FilterEquals:
        return i18nc( "@item:inlistbox Name of the filter variant that matches if "
                      "the filter word is found as complete text (not only contained) or "
                      "if the filter value is equal for non-string-filters", "Equals" );
    case FilterDoesntEqual:
        return i18nc( "@item:inlistbox Name of the filter variant that matches if "
                      "the filter word is not found as complete text (or only contained) or "
                      "if the filter value is not equal for non-string-filters", "Does not Equal" );
    case FilterMatchesRegExp:
        return i18nc( "@item:inlistbox Name of the filter variant that matches if "
                      "a regular expression matches", "Matches Regular Expr." );
    case FilterDoesntMatchRegExp:
        return i18nc( "@item:inlistbox Name of the filter variant that matches if "
                      "a regular expression doesn't match", "Doesn't Match Reg. Expr." );

    case FilterIsOneOf:
        return i18nc( "@item:inlistbox Name of the filter variant that matches if "
                      "a value is contained in a list of values, eg. strings.", "One of" );
    case FilterIsntOneOf:
        return i18nc( "@item:inlistbox Name of the filter variant that matches if "
                      "a value is not contained in a list of values, eg. strings.", "None of" );

    case FilterGreaterThan:
        return i18nc( "@item:inlistbox Name of the filter variant that matches if "
                      "a value is greater than the filter value.", "Greater Than" );
    case FilterLessThan:
        return i18nc( "@item:inlistbox Name of the filter variant that matches if "
                      "a value is less than the filter value.", "Less Than" );

    default:
        kDebug() << "Filter variant unknown" << filterVariant;
        return QString();
    }
}

ConstraintWidget::ConstraintWidget( FilterType type, QList< FilterVariant > availableVariants,
                                    FilterVariant initialVariant, QWidget* parent )
        : QWidget( parent )
{
    m_constraint.type = type;
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
    QFormLayout *layout = new QFormLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setRowWrapPolicy( QFormLayout::WrapLongRows );
    setLayout( layout );

    if ( !availableVariants.isEmpty() ) {
        if ( !availableVariants.contains( initialVariant ) ) {
            initialVariant = availableVariants.first();
            kDebug() << "Initial variant" << initialVariant
                     << "not found in" << availableVariants << "for type" << type;
            kDebug() << "Using first available variant as initial variant:" << initialVariant;
        }

        m_variantsCmb = new KComboBox( this );
        m_variantsCmb->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
        foreach( const FilterVariant &variant, availableVariants ) {
            m_variantsCmb->addItem( filterVariantName(variant), static_cast<int>(variant) );
        }
        int index = m_variantsCmb->findData( static_cast<int>( initialVariant ) );
        connect( m_variantsCmb, SIGNAL(currentIndexChanged(int)), this, SLOT(variantChanged(int)) );
        m_variantsCmb->setCurrentIndex( index );
        m_constraint.variant = initialVariant;
        layout->addWidget( m_variantsCmb );
    } else {
        m_constraint.variant = FilterNoVariant;
    }
}

ConstraintListWidget::ConstraintListWidget( FilterType type, FilterVariant initialVariant,
        const QList< ListItem > &values, const QVariantList& initialValues, QWidget* parent )
        : ConstraintWidget( type, QList< FilterVariant >() << FilterIsOneOf << FilterIsntOneOf,
                            initialVariant, parent )
{
    m_list = new CheckCombobox( this );
    QStandardItemModel *model = new QStandardItemModel( this );
    foreach( const ListItem &listItem, values ) {
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

    connect( m_list, SIGNAL(checkedItemsChanged()), this, SLOT(checkedItemsChanged()) );
}

ConstraintStringWidget::ConstraintStringWidget( FilterType type, FilterVariant initialVariant,
        const QString& filterText, QWidget* parent )
        : ConstraintWidget( type, QList< FilterVariant >()
            << FilterContains << FilterDoesntContain << FilterEquals << FilterDoesntEqual
            << FilterMatchesRegExp << FilterDoesntMatchRegExp,
            initialVariant, parent )
{
    m_string = new KLineEdit( this );
    m_string->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    m_string->setClearButtonShown( true );
    m_string->setText( filterText );
    m_string->setMinimumWidth( 100 );
    addWidget( m_string );

    connect( m_string, SIGNAL(textChanged(QString) ), this, SLOT(stringChanged(QString)) );
}

ConstraintIntWidget::ConstraintIntWidget( FilterType type, FilterVariant initialVariant,
        int value, int min, int max, QWidget* parent )
        : ConstraintWidget( type, QList< FilterVariant >()
            << FilterEquals << FilterDoesntEqual << FilterGreaterThan << FilterLessThan,
            initialVariant, parent )
{
    m_num = new KIntSpinBox( this );
    m_num->setRange( min, max );
    m_num->setValue( value );
    addWidget( m_num );

    connect( m_num, SIGNAL(valueChanged(int)), this, SLOT(intChanged(int)) );
}

ConstraintTimeWidget::ConstraintTimeWidget( FilterType type,
        FilterVariant initialVariant, QTime value, QWidget* parent )
        : ConstraintWidget( type, QList< FilterVariant >()
            << FilterEquals << FilterDoesntEqual << FilterGreaterThan << FilterLessThan,
            initialVariant, parent )
{
    value.setHMS( value.hour(), value.minute(), 0 );
    m_time = new QTimeEdit( value, this );
    addWidget( m_time );

    connect( m_time, SIGNAL(timeChanged(QTime)), this, SLOT(timeChanged(QTime)) );
}

ConstraintDateWidget::ConstraintDateWidget( FilterType type, FilterVariant initialVariant,
        QDate value, QWidget* parent )
        : ConstraintWidget( type, QList< FilterVariant >()
            << FilterEquals << FilterDoesntEqual << FilterGreaterThan << FilterLessThan,
            initialVariant, parent )
{
    m_date = new KDateComboBox( this );
    m_date->setDate( value );
    addWidget( m_date );

    connect( m_date, SIGNAL(dateChanged(QDate)), this, SLOT(dateChanged(QDate)) );
}

void ConstraintListWidget::setValue( const QVariant& value )
{
    QModelIndexList indices;
    if ( value.isValid() ) {
        QVariantList values = value.toList();
        foreach( const QVariant &value, values ) {
            QModelIndex index = indexFromValue( value );
            if ( index.isValid() ) {
                indices << index;
            } else {
                kDebug() << "Value" << value << "not found";
            }
        }
    }

    m_list->setCheckedItems( indices );
}

QModelIndex ConstraintListWidget::indexFromValue( const QVariant& value )
{
    QModelIndexList indices = m_list->model()->match(
            m_list->model()->index(0, 0), Qt::UserRole, value, 1, Qt::MatchExactly );

    return indices.isEmpty() ? QModelIndex() : indices.first();
}

FilterListWidget::FilterListWidget( QWidget* parent ) : AbstractDynamicWidgetContainer(
        parent, RemoveButtonsBesideWidgets, AddButtonAfterLastWidget, ShowSeparators,
        AddWidgetsAtBottom )
{
    setWidgetCountRange( 1, 10, false );
    addButton()->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    addButton()->setText( i18nc("@action:button", "&Add Filter") );
    addButton()->setToolTip( i18nc("@info:tooltip", "Add another filter (logical OR)") );
}

void ConstraintWidget::variantChanged( int index )
{
    FilterVariant newVariant = static_cast<FilterVariant>( m_variantsCmb->itemData(index).toInt() );

    if ( m_constraint.variant != newVariant ) {
        m_constraint.variant = newVariant;
        emit changed();
    }
}

QWidget* FilterWidget::createNewLabelWidget( int )
{
    // Create a new combobox which contains all allowed filter types as label widget
    KComboBox *cmbFilterType = new KComboBox( this );
    foreach( FilterType filterType, m_allowedFilterTypes ) {
        cmbFilterType->addItem( filterName(filterType) + ':', static_cast<int>(filterType) );
    }
    return cmbFilterType;
}

QWidget* FilterWidget::createSeparator( const QString& separatorText )
{
    return AbstractDynamicWidgetContainer::createSeparator(
            separatorText.isEmpty() ? m_separatorText : separatorText );
}

DynamicWidget* FilterWidget::addWidget( QWidget* labelWidget, QWidget* widget )
{
    KComboBox *cmbFilterType = qobject_cast< KComboBox* >( labelWidget );
    Q_ASSERT_X( cmbFilterType, "FilterWidget::addWidget",
                "Wrong label widget type or NULL label widget" );
    DynamicWidget *dynamicWidget =
            AbstractDynamicLabeledWidgetContainer::addWidget( labelWidget, widget );
    if ( dynamicWidget ) {
        // Add combobox to the list of filter type widgets
        m_filterTypes << cmbFilterType;

        // Set correct filter type in the combobox
        ConstraintWidget *constraintWidget = dynamicWidget->contentWidget< ConstraintWidget* >();
        cmbFilterType->setCurrentIndex( cmbFilterType->findData(
                                            static_cast<int>(constraintWidget->type())) );

        // Connect changed signals
        connect( cmbFilterType, SIGNAL(currentIndexChanged(int)),
                this, SLOT(filterTypeChanged(int)) );
        connect( constraintWidget, SIGNAL(changed()), this, SIGNAL(changed()) );

        // Set tooltip for add/remove buttons, if any
        if ( dynamicWidget->removeButton() ) {
            dynamicWidget->removeButton()->setToolTip(
                    i18nc( "@info:tooltip", "Remove this criterion from the filter" ) );
        }
        if ( dynamicWidget->addButton() ) {
            dynamicWidget->addButton()->setToolTip(
                    i18nc( "@info:tooltip", "Add another criterion to this filter (logical AND)" ) );
        }

        emit changed();
        emit constraintAdded( constraintWidget );
    }
    return dynamicWidget;
}

DynamicWidget* FilterListWidget::addWidget( QWidget* widget )
{
    DynamicWidget *newWidget = AbstractDynamicWidgetContainer::addWidget( widget );
    if ( newWidget->removeButton() ) {
        newWidget->removeButton()->setToolTip(
                i18nc( "@info:tooltip", "Remove this filter with all it's criteria" ) );
    }

    emit changed();
    return newWidget;
}

DynamicWidget* FilterListWidget::createDynamicWidget( QWidget* widget )
{
    // Set spacing between the contained constraint list and the remove filter button
    DynamicWidget *dynamicWidget = AbstractDynamicWidgetContainer::createDynamicWidget( widget );
    dynamicWidget->layout()->setSpacing( 1 );
    return dynamicWidget;
}

QWidget* FilterListWidget::createSeparator( const QString& separatorText )
{
    return AbstractDynamicWidgetContainer::createSeparator(
            separatorText.isEmpty() ? i18nc("@info/plain", "or") : separatorText );
}

QList< FilterWidget* > FilterListWidget::filterWidgets() const
{
    QList< FilterWidget* > list;
    foreach( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
        list << qobject_cast< FilterWidget* >( dynamicWidget->contentWidget() );
    }
    return list;
}

FilterList FilterListWidget::filters() const
{
    // Get the filter of each FilterWidget
    FilterList list;
    foreach( DynamicWidget *dynamicWidget, dynamicWidgets() ) {
        list << qobject_cast< FilterWidget* >( dynamicWidget->contentWidget() )->filter();
    }
    return list;
}

void ConstraintListWidget::checkedItemsChanged()
{
    // Update list of values of checked items
    m_values.clear();
    foreach( const QModelIndex &index, m_list->checkedItems() ) {
        m_values << index.data( Qt::UserRole );
    }

    emit changed();
}

void ConstraintWidget::addWidget( QWidget* w ) {
    QFormLayout *l = dynamic_cast< QFormLayout* >( layout() );
    QLayoutItem *item = l->itemAt( 0 );
    if ( item ) {
        l->removeItem( item );
        l->addRow( item->widget(), w );
    }
}

Filter FilterWidget::filter() const
{
    Filter f;
    foreach( ConstraintWidget *constraint, constraintWidgets() ) {
        f << constraint->constraint();
    }
    return f;
}

FilterWidget* FilterWidget::create( const PublicTransport::Filter& filter, QWidget* parent )
{
    FilterWidget *filterWidget = new FilterWidget( QList<FilterType>()
            << FilterByVehicleType << FilterByTarget << FilterByVia << FilterByNextStop
            << FilterByTransportLine << FilterByTransportLineNumber << FilterByDelay, parent );
    filterWidget->setFilter( filter );
    return filterWidget;
}

void FilterListWidget::addFilter()
{
    Filter filter;
    filter << Constraint();
    addFilter( filter );
}

FilterListWidget* FilterListWidget::create( const PublicTransport::FilterList& filterList, QWidget* parent )
{
    FilterListWidget *filterListWidget = new FilterListWidget( parent );
    foreach( Filter filter, filterList ) {
        filterListWidget->addFilter( filter );
    }
    return filterListWidget;
}

QVariant ConstraintTimeWidget::value() const
{
    return m_time->time();
}

void ConstraintTimeWidget::setValue( const QVariant& value )
{
    m_time->setTime( value.toTime() );
}

QVariant ConstraintDateWidget::value() const
{
    return m_date->date();
}

void ConstraintDateWidget::setValue( const QVariant& value )
{
    m_date->setDate( value.toDate() );
}

QVariant ConstraintIntWidget::value() const
{
    return m_num->value();
}

void ConstraintIntWidget::setValue( const QVariant& value )
{
    m_num->setValue( value.toInt() );
}

QVariant ConstraintStringWidget::value() const
{
    return m_string->text();
}

void ConstraintStringWidget::setValue( const QVariant& value )
{
    m_string->setText( value.toString() );
}

} // namespace Timetable
