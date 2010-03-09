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

#ifndef FILTERWIDGET_HEADER
#define FILTERWIDGET_HEADER

#include <QWidget>
#include <QCheckBox>
#include <QToolButton>
#include <QListView>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QApplication>
#include <QStandardItemModel>

#include <KLineEdit>
#include <KNumInput>
#include <KDebug>

// TODO: Forward Declarations?
#include "global.h"
#include "checkcombobox.h"
#include "dynamicwidget.h"
#include "filter.h"

class ConstraintWidget : public QWidget {
    Q_OBJECT

    public:
	ConstraintWidget( FilterType type, QList<FilterVariant> availableVariants = QList<FilterVariant>(),
			  FilterVariant initialVariant = FilterNoVariant, QWidget* parent = 0 );

	FilterType type() const { return m_constraint.type; };
	FilterVariant variant() const { return m_constraint.variant; };
	virtual QVariant value() const = 0;
	virtual void setValue( const QVariant &value ) = 0;
	Constraint constraint() {
	    m_constraint.value = value();
	    return m_constraint; };
	
	void addWidget( QWidget *w ) {
	    w->setVisible( true ); //m_variant != FilterDisregard );
	    layout()->addWidget( w );
	    m_widgets << w;
	};

	QWidget *containerWidget() const { return m_containerWidget; };
	QWidgetList permanentWidgets() const { return m_permanentWidgets; };
	void addPermanentWidgets( QWidgetList widgets ) {
	    foreach ( QWidget *w, widgets )
		addPermanentWidget( w ); };
	void addPermanentWidget( QWidget *w ) {
// 	    if ( m_permanentWidgets.isEmpty() )
// 		layout()->addItem( new QSpacerItem(0, 0, QSizePolicy::Expanding) );
	    if ( !m_containerWidget ) {
		m_containerWidget = new QWidget( this );
		layout()->addWidget( m_containerWidget );
		layout()->setAlignment( m_containerWidget, Qt::AlignRight );
		m_containerWidget->setLayout( new QHBoxLayout(m_containerWidget) );
		m_containerWidget->layout()->setSpacing( 1 );
		m_containerWidget->layout()->setContentsMargins( 0, 0, 0, 0 );
	    }
	    m_containerWidget->layout()->addWidget( w );
	    m_permanentWidgets << w; };
	void removePermanentWidgets() {
	    foreach ( QWidget *w, m_permanentWidgets )
		m_containerWidget->layout()->removeWidget( w );
	    m_permanentWidgets.clear(); };
	void removePermanentWidget( QWidget *w ) {
	    m_containerWidget->layout()->removeWidget( w );
	    m_permanentWidgets.removeOne( w ); };

	inline static ConstraintWidget *create( Constraint constraint,
						QWidget *parent = 0 ) {
	    return create( constraint.type, constraint.variant,
			   constraint.value, parent );
	};
	static ConstraintWidget *create( FilterType type,
					 FilterVariant variant = FilterNoVariant,
					 const QVariant &value = QVariant(),
					 QWidget *parent = 0 );

    signals:
	void changed();
					 
    protected slots:
	virtual void variantChanged( int index ) {
	    FilterVariant newVariant = static_cast< FilterVariant >(
		    m_variantsCmb->itemData(index).toInt() );

	    if ( m_constraint.variant != newVariant ) {
		m_constraint.variant = newVariant;
		bool visible = true; //m_variant != FilterDisregard;
		foreach ( QWidget *w, m_widgets )
		    w->setVisible( visible );
		emit changed();
	    }
	};
	
    private:
	QString filterVariantName( FilterVariant filterVariant ) const;

	Constraint m_constraint;
	KComboBox* m_variantsCmb;
	QWidget *m_containerWidget;
	QWidgetList m_widgets, m_permanentWidgets;
};

class ConstraintListWidget : public ConstraintWidget {
    Q_OBJECT

    public:
	struct ListItem {
	    QString text;
	    QVariant value;
	    KIcon icon;

	    ListItem() {};
	    ListItem( const QString &text, const QVariant &value, const KIcon &icon ) {
		this->text = text;
		this->value = value;
		this->icon = icon; };
	};
	
	ConstraintListWidget( FilterType type, FilterVariant initialVariant,
			  const QList< ListItem > &values,
			  const QVariantList& initialValues,
			  QWidget* parent = 0 );
			  
	virtual QVariant value() const { return m_values; };
	virtual void setValue( const QVariant &value );
	
	QModelIndex indexFromValue( const QVariant &value );

    protected slots:
	void checkedItemsChanged() {
	    m_values.clear();
	    foreach ( QModelIndex index, m_list->checkedItems() )
		m_values << index.data( Qt::UserRole );
	    emit changed();
	    kDebug() << "EMIT CHANGED";
	};

    private:
	CheckCombobox *m_list;
	QVariantList m_values;
};

class ConstraintStringWidget : public ConstraintWidget {
    Q_OBJECT

    public:
	ConstraintStringWidget( FilterType type, FilterVariant initialVariant,
				const QString &filterText = QString(),
				QWidget* parent = 0 );
			    
	virtual QVariant value() const { return m_string->text(); };
	virtual void setValue( const QVariant &value ) { m_string->setText(value.toString()); };
	
    protected slots:
	void stringChanged( const QString &newString ) {
	    Q_UNUSED( newString);
	    emit changed();
	};
	
    private:
	KLineEdit *m_string;
};

class ConstraintIntWidget : public ConstraintWidget {
    Q_OBJECT

    public:
	ConstraintIntWidget( FilterType type, FilterVariant initialVariant,
			 int value = 0, int min = 0, int max = 10000, QWidget* parent = 0 );

	virtual QVariant value() const { return m_num->value(); };
	virtual void setValue( const QVariant &value ) { m_num->setValue(value.toInt()); };
	    
    protected slots:
	void intChanged( int newInt ) {
	    Q_UNUSED( newInt );
	    emit changed();
	};
	
    private:
	KIntSpinBox *m_num;
};

class FilterWidget : public AbstractDynamicLabeledWidgetContainer {
    Q_OBJECT

    public:
	FilterWidget( QWidget* parent = 0 );
	FilterWidget( QList<FilterType> filterTypes, QWidget* parent = 0 );

	QList< ConstraintWidget* > constraintWidgets() const {
	    QList< ConstraintWidget* > list;
	    foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() )
		list << qobject_cast< ConstraintWidget* >( dynamicWidget->contentWidget() );
	    return list;
	};
	Filter filter() const {
	    Filter f;
	    foreach ( ConstraintWidget *constraint, constraintWidgets() )
		f << constraint->constraint();
	    return f;
	};

	inline void addConstraint( FilterType filterType ) {
	    addConstraint( createFilter(filterType) ); };
	void addConstraint( const Constraint &constraint ) {
	    addConstraint( ConstraintWidget::create(constraint, this) );
	};
	void addConstraint( ConstraintWidget *constraint );
	virtual int removeWidget( QWidget* widget );

	static FilterWidget *create( const Filter &filter, QWidget *parent = 0 ) {
	    FilterWidget *filterWidget = new FilterWidget( parent );
	    foreach ( Constraint constraint, filter )
		filterWidget->addConstraint( constraint );
	    return filterWidget;
	};
	
    signals:
	void changed();
	void constraintAdded( ConstraintWidget *constraintWidget );
	void constraintRemoved( const Constraint &constraint );

    public slots:
	inline void addConstraint() {
	    addConstraint( qobject_cast<ConstraintWidget*>(createNewWidget()) ); };
	void removeConstraint( ConstraintWidget *widget );

    protected slots:
	void filterTypeChanged( int index );
// 	virtual void createAndAddWidget() { addConstraint(); };

    protected:
	virtual QWidget *createNewWidget() {
	    return createFilter( firstUnusedFilterType() );
	};
	
	virtual QWidget* createNewLabelWidget( int ) {
	    KComboBox *cmbFilterType = new KComboBox( this );
	    cmbFilterType->addItem( filterName(FilterByVehicleType) + ":",
				    static_cast<int>(FilterByVehicleType) );
	    cmbFilterType->addItem( filterName(FilterByTransportLine) + ":",
				    static_cast<int>(FilterByTransportLine) );
	    cmbFilterType->addItem( filterName(FilterByTransportLineNumber) + ":",
				    static_cast<int>(FilterByTransportLineNumber) );
	    cmbFilterType->addItem( filterName(FilterByTarget) + ":",
				    static_cast<int>(FilterByTarget) );
	    cmbFilterType->addItem( filterName(FilterByDelay) + ":",
				    static_cast<int>(FilterByDelay) );
	    return cmbFilterType;
	};
	
	virtual DynamicWidget *addWidget( QWidget *widget ) {
	    return AbstractDynamicLabeledWidgetContainer::addWidget( widget ); };
	
	virtual DynamicWidget *addWidget( QWidget *labelWidget, QWidget *widget ) {
	    KComboBox *cmbFilterType = qobject_cast< KComboBox* >( labelWidget );
	    Q_ASSERT_X( cmbFilterType, "FilterWidget::addWidget",
			"Wrong label widget type or NULL label widget" );
	    DynamicWidget *dynamicWidget =
		AbstractDynamicLabeledWidgetContainer::addWidget( labelWidget, widget );
	    if ( dynamicWidget ) {
		m_filterTypes << cmbFilterType;

		ConstraintWidget *constraintWidget =
			dynamicWidget->contentWidget< ConstraintWidget* >();
		cmbFilterType->setCurrentIndex( cmbFilterType->findData(
			static_cast<int>(constraintWidget->type())) );
			
		connect( cmbFilterType, SIGNAL(currentIndexChanged(int)),
			this, SLOT(filterTypeChanged(int)) );
		connect( constraintWidget, SIGNAL(changed()), this, SIGNAL(changed()) );
		
		if ( dynamicWidget->removeButton() )
		    dynamicWidget->removeButton()->setToolTip(
			    i18n("Remove this criterion from the filter") );
		if ( dynamicWidget->addButton() )
		    dynamicWidget->addButton()->setToolTip(
			    i18n("Add another criterion to this filter") );
		
		emit changed();
		emit constraintAdded( constraintWidget );
	    }
	    return dynamicWidget;
	};

	virtual void updateLabelWidget( QWidget*, int ) {};

    private:
	ConstraintWidget *createFilter( FilterType type );
	QString filterName( FilterType filter ) const;
	FilterType firstUnusedFilterType() const;
	
	QList< KComboBox* > m_filterTypes;
};

class FilterListWidget : public AbstractDynamicWidgetContainer {
    Q_OBJECT
    public:
	FilterListWidget( QWidget* parent = 0 );
	
	QList< FilterWidget* > filterWidgets() const {
	    QList< FilterWidget* > list;
	    foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() )
		list << qobject_cast< FilterWidget* >( dynamicWidget->contentWidget() );
	    return list;
	};

	FilterList filters() const {
	    FilterList list;
	    foreach ( DynamicWidget *dynamicWidget, dynamicWidgets() )
		list << qobject_cast< FilterWidget* >( dynamicWidget->contentWidget() )->filter();
	    return list;
	};

	void addFilter() {
	    Filter filter;
	    filter << Constraint();
	    addFilter( filter );
	};

	inline void addFilter( const Filter &filter ) {
	    addFilter( FilterWidget::create(filter, this) );
	};
	
	void addFilter( FilterWidget *filterWidget ) {
	    connect( filterWidget, SIGNAL(changed()), this, SIGNAL(changed()) );
	    addWidget( filterWidget );
	};
	
	static FilterListWidget *create( const FilterList &filterList, QWidget *parent = 0 ) {
	    FilterListWidget *filterListWidget = new FilterListWidget( parent );
	    foreach ( Filter filter, filterList )
		filterListWidget->addFilter( filter );
	    return filterListWidget;
	};
	
    signals:
	void changed();
	
    protected:
	virtual QWidget *createNewWidget() {
	    Filter filter;
	    filter << Constraint();
	    return FilterWidget::create( filter, this );
	};
	
	virtual DynamicWidget *addWidget( QWidget* widget ) {
	    DynamicWidget *newWidget = AbstractDynamicWidgetContainer::addWidget( widget );
	    if ( newWidget->removeButton() )
		newWidget->removeButton()->setToolTip(
			i18n("Remove this filter with all it's criteria") );
	    
	    emit changed();
	    return newWidget;
	};
	
	virtual int removeWidget( QWidget* widget ) {
	    int index = AbstractDynamicWidgetContainer::removeWidget( widget );
	    emit changed();
	    return index;
	};
	
	virtual QWidget *createSeparator( const QString &separatorText = QString() ) {
	    return AbstractDynamicWidgetContainer::createSeparator(
		    separatorText.isEmpty() ? i18n("or") : separatorText );
	};
};

#endif // Multiple inclusion guard