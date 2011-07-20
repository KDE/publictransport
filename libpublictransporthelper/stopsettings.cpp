/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

#include "stopsettings.h"
#include "checkcombobox.h"
#include "filter.h"

#include <KLocale>
#include <KGlobal>
#include <KDebug>

#include <QString>
#include <QTime>
#include <QWidget>
#include <QLabel>
#include <QSpinBox>
#include <QTimeEdit>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QRadioButton>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

Stop::Stop()
{
}

Stop::Stop(const char *name)
{
	this->name = name;
}

Stop::Stop(const QLatin1String& name)
{
	this->name = name;
}

Stop::Stop(const QString& name, const QString& id)
{
	this->name = name;
	this->id = id;
}

Stop::Stop(const Stop& other)
{
	this->name = other.name;
	this->id = other.id;
}

Stop::~Stop()
{
}

Stop& Stop::operator=(const Stop &other)
{
	name = other.name;
	id = other.id;
	return *this;
}

bool Stop::operator==(const Stop& other) const
{
	if ( id.isEmpty() || other.id.isEmpty() ) {
		// An ID is missing from this and/or the other StopName, don't compare them
		return name == other.name;
	} else {
		// If both IDs are used they should be the same
		return name == other.name && id == other.id;
	}
}

class StopSettingsPrivate : public QSharedData {
public:
	StopSettingsPrivate() {
		settings[ LocationSetting ] = KGlobal::locale()->country();
	};
	
	StopSettingsPrivate( const QHash<int, QVariant>& data ) : settings(data) {
	};
    
	StopSettingsPrivate( const StopSettingsPrivate &other )
		: QSharedData(other), settings(other.settings) 
	{ 
	};

	
	QHash< int, QVariant > settings;
};

StopSettings::StopSettings() : d(new StopSettingsPrivate())
{
}

StopSettings::StopSettings(const QHash<int, QVariant>& data) 
		: d(new StopSettingsPrivate(data))
{
}

StopSettings::StopSettings(const StopSettings& other) : d(other.d)
{
}

StopSettings::~StopSettings()
{
}

QStringList StopSettings::stops(StopSettings::StopIdUsage stopIdUsage) const
{
// 	Q_D( const StopSettings );
// 	return d->stops;
	StopList stops = stopList();
	QStringList ret;
	if ( stopIdUsage == StopSettings::UseStopIdIfAvailable ) {
		foreach ( const Stop &stop, stops ) {
			ret << stop.nameOrId();
		}
	} else {
		foreach ( const Stop &stop, stops ) {
			ret << stop.name;
		}
	}
	return ret;
}

const StopList StopSettings::stopList() const
{
	return d->settings[ StopNameSetting ].value<StopList>();
}

const Stop StopSettings::stop(int index) const
{
// 	Q_D( const StopSettings );
// 	return d->stops[index];
	return stopList().at( index );
}

QStringList StopSettings::stopIDs() const
{
// 	Q_D( const StopSettings );
// 	return d->stopIDs;
	StopList stops = stopList();
	QStringList ret;
	foreach ( const Stop &stop, stops ) {
		ret << stop.id;
	}
	return ret;
}

bool StopSettings::hasSetting( int setting ) const
{
	return d->settings.contains( setting );
}

QList< int > StopSettings::usedSettings() const
{
	return d->settings.keys();
}

QHash< int, QVariant > StopSettings::settings() const
{
	return d->settings;
}

const QVariant StopSettings::operator[](int setting) const
{
	return d->settings[ setting ];
}

QVariant& StopSettings::operator[](int setting)
{
	return d->settings[ setting ];
}

const QVariant StopSettings::operator[](StopSetting setting) const
{
	return d->settings[ static_cast<int>(setting) ];
}

QVariant& StopSettings::operator[](StopSetting setting)
{
	return d->settings[ static_cast<int>(setting) ];
}

void StopSettings::setStop(const Stop& stop)
{
	d->settings[ StopNameSetting ] = QVariant::fromValue<StopList>( StopList() << stop );
}

void StopSettings::setStops(const QStringList& stops, const QStringList& stopIDs)
{
	StopList stopList;
	if ( stops.count() == stopIDs.count() ) {
		for ( int i = 0; i < stops.count(); ++i ) {
			stopList << Stop( stops[i], stopIDs[i] );
		}
	} else {
		foreach ( const QString &stop, stops ) {
			stopList << Stop( stop );
		}
	}
	setStops( stopList );
}

void StopSettings::setStops(const StopList& stopNameList)
{
	d->settings[ StopNameSetting ] = QVariant::fromValue<StopList>( stopNameList );
}

void StopSettings::setIdOfStop(const QString& stop, const QString& id)
{
	StopList stopNames = d->settings[ StopNameSetting ].value<StopList>();
	int index = stopNames.indexOf( Stop(stop) );
	if ( index >= 0 ) {
		stopNames[ index ].id = id;
		d->settings[ StopNameSetting ] = QVariant::fromValue<StopList>( stopNames );
	} else {
		kDebug() << "Couldn't finde stop" << stop << "to set it's ID to" << id;
	}
}

void StopSettings::set( int setting, const QVariant &value )
{
	d->settings[ setting ] = value;
}

void StopSettings::clearSetting( int setting )
{
	d->settings.remove( setting );
}

StopSettings& StopSettings::operator=(const StopSettings& rhs)
{
	if ( this == &rhs ) {
		return *this; // Protect against self-assignment
	}
	
	d = rhs.d;
	return *this;
}

bool StopSettings::operator==(const StopSettings& other) const {
	if ( d->settings.count() != other.d->settings.count() ) {
		return false;
	}

	// Go through all settings
	for ( QHash<int, QVariant>::const_iterator it = d->settings.constBegin(); 
		 it != d->settings.constEnd(); ++it )
	{
        // StopNameSetting and FilterConfigurationSetting need special handling, because they 
        // use a custom type (StopList, FilterSettingsList).
        // QVariant doesn't compare values of custom types, but addresses instead.
		if ( it.key() == StopNameSetting ) {
            if ( it.value().value<StopList>() != other.d->settings[it.key()].value<StopList>() ) {
                return false;
            }
        } else if ( it.key() == FilterConfigurationSetting ) {
            if ( it.value().value<FilterSettingsList>()
                 != other.d->settings[it.key()].value<FilterSettingsList>() )
            {
                return false;
            }
        } else if ( it.key() >= UserSetting ) {
			continue; // Can't compare custom QVariant types, addresses would get compared
		} else if ( it.value() != other.d->settings[it.key()] ) {
			return false;
		}
	}
	
	// No differences found
	return true;
}

void StopSettingsList::removeIntermediateSettings( int startIndex,
        const QString& id, int stopSetting )
{
    for ( int i = startIndex; i < count(); ++i ) {
        if ( operator[](i).get<QString>(stopSetting).compare(id) == 0 ) {
            kDebug() << "Found at" << i;
            removeAt( i );
            --i;
        }
    }
}

int StopSettingsList::findStopSettings( const QString& stopName, int startIndex )
{
    for ( int i = startIndex; i < count(); ++i ) {
        if ( operator[](i).stops().contains(stopName, Qt::CaseInsensitive) ) {
            return i;
        }
    }

    return -1; // Not found
}

StopSettingsWidgetFactory::~StopSettingsWidgetFactory()
{
}

QString StopSettingsWidgetFactory::textForSetting( int setting ) const
{
	switch ( setting ) {
		case FilterConfigurationSetting:
			return i18nc("@label:listbox", "&Filter Configurations:");
		case AlarmTimeSetting:
			return i18nc("@label:spinbox", "A&larm Time:");
		case FirstDepartureConfigModeSetting:
			return i18nc("@label", "&First Departure:");
		case TimeOffsetOfFirstDepartureSetting:
			return i18nc("@label:spinbox", "&Relative to Current Time:");
		case TimeOfFirstDepartureSetting:
			return i18nc("@label", "At &Custom Time:");
			
		default:
			if ( setting >= UserSetting ) {
				kDebug() << "No text defined for custom setting" << static_cast<StopSetting>(setting);
			} else {
				kDebug() << "Intern error: No text defined for setting" << static_cast<StopSetting>(setting);
			}
			return NULL;
	}
}

QString StopSettingsWidgetFactory::nameForSetting( int setting ) const
{
	switch ( setting ) {
		case FilterConfigurationSetting:
			return "filterConfiguration";
		case AlarmTimeSetting:
			return "alarmTime";
		case FirstDepartureConfigModeSetting:
			return "firstDepartureConfigMode";
		case TimeOffsetOfFirstDepartureSetting:
			return "timeOffsetOfFirstDeparture";
		case TimeOfFirstDepartureSetting:
			return "timeOfFirstDeparture";
			
		default:
			if ( setting >= UserSetting ) {
				kDebug() << "No name defined for custom setting" << static_cast<StopSetting>(setting)
						 << " - Using" << (QLatin1String("UserSetting_") + QString::number(setting)) 
						 << "instead";
				return QLatin1String("UserSetting_") + QString::number(setting);
			} else {
				kDebug() << "Intern error: No name defined for setting" << static_cast<StopSetting>(setting);
				return QString();
			}
	}
}

bool StopSettingsWidgetFactory::isDetailsSetting( int setting ) const
{
	switch ( setting ) {
		case LocationSetting:
		case ServiceProviderSetting:
		case CitySetting:
		case StopNameSetting:
			return false;
			break;
	
		case FilterConfigurationSetting:
		case AlarmTimeSetting:
		case FirstDepartureConfigModeSetting:
		case TimeOffsetOfFirstDepartureSetting:
		case TimeOfFirstDepartureSetting:
		default:
			return true;
	}
}

QVariant StopSettingsWidgetFactory::valueOfSetting( const QWidget* widget, int setting,
                                                    int stopIndex ) const
{
	switch ( setting ) {
		case FilterConfigurationSetting: {
            // Get filter configuration list and adjust affectedStops list
            FilterSettingsList filterSettings;
            const CheckCombobox *filterConfiguration = qobject_cast< const CheckCombobox* >( widget );
            QAbstractItemModel *model = filterConfiguration->model();
            QList<int> checkedFilterConfigurations = filterConfiguration->checkedRows();
            for ( int row = 0; row < model->rowCount(); ++row ) {
                FilterSettings filter = model->data( model->index(row, 0), FilterSettingsRole )
                        .value<FilterSettings>();

                if ( stopIndex != -1 ) {
                    if ( checkedFilterConfigurations.contains(row) ) {
                        filter.affectedStops << stopIndex;
                    } else if ( filter.affectedStops.contains(stopIndex) ) {
                        filter.affectedStops.remove( stopIndex );
                    }
                }

                filterSettings << filter;
            }

            return QVariant::fromValue( filterSettings );
		}
		case AlarmTimeSetting:
		case TimeOffsetOfFirstDepartureSetting: {
			return qobject_cast< const QSpinBox* >( widget )->value();
		}
		case FirstDepartureConfigModeSetting: {
			QRadioButton *timeOffsetRadio = widget->parentWidget()->findChild<QRadioButton*>(
					QLatin1String("radio_") + 
					nameForSetting(TimeOffsetOfFirstDepartureSetting) );
			return timeOffsetRadio 
					? (timeOffsetRadio->isChecked() ? RelativeToCurrentTime : AtCustomTime) 
					: QVariant();
		}
		case TimeOfFirstDepartureSetting: {
			return qobject_cast< const QTimeEdit* >( widget )->time();
		}
		default:
			if ( setting >= UserSetting ) {
				kDebug() << "Getting the value of the widget defined for custom setting" 
						 << static_cast<StopSetting>(setting) << "not implemented";
			} else {
				kDebug() << "Intern error: No code to get the value of the widget defined for "
							"setting" << static_cast<StopSetting>(setting);
			}
			return QVariant();
	}
}

void StopSettingsWidgetFactory::setValueOfSetting(QWidget* widget, int setting, 
												  const QVariant& value) const
{
	switch ( setting ) {
		case FilterConfigurationSetting: {
//             TODO: Not used currently?
            FilterSettingsList filterSettings = value.value<FilterSettingsList>();
            const CheckCombobox *filterConfiguration = qobject_cast<CheckCombobox*>( widget );
            QAbstractItemModel *model = filterConfiguration->model();
            int row = 0;
            foreach ( const FilterSettings &filter, filterSettings ) {
                model->insertRow( row );
                QModelIndex index = model->index( row, 0 );
                model->setData( index, filter.name, Qt::DisplayRole );
                model->setData( index, QVariant::fromValue(filter), FilterSettingsRole );
                ++row;
            }
//             filterConfiguration->setCheckedTexts(  ); // TODO
			break;
        }
		case AlarmTimeSetting:
		case TimeOffsetOfFirstDepartureSetting:
			qobject_cast< QSpinBox* >( widget )->setValue( value.toInt() );
			break;
			
		case FirstDepartureConfigModeSetting: {
			FirstDepartureConfigMode configMode = 
					static_cast<FirstDepartureConfigMode>(value.toInt());
			StopSetting setting = configMode == RelativeToCurrentTime
					? TimeOffsetOfFirstDepartureSetting
					: TimeOfFirstDepartureSetting;
			QRadioButton *radio = widget->parentWidget()->findChild<QRadioButton*>(
					QLatin1String("radio_") + nameForSetting(setting) );
			if ( radio ) {
				radio->setChecked( true );
			}
			break;
		}
		case TimeOfFirstDepartureSetting:
			return qobject_cast< QTimeEdit* >( widget )->setTime( value.toTime() );
			
		default:
			if ( setting >= UserSetting ) {
				kDebug() << "Setting the value of the widget defined for custom setting" 
						 << static_cast<StopSetting>(setting) << "not implemented";
			} else {
				kDebug() << "Intern error: No code to set the value of the widget defined for "
							"setting" << static_cast<StopSetting>(setting);
			}
			break;
	}
}

QWidget* StopSettingsWidgetFactory::widgetForSetting( int setting, QWidget *parent ) const
{
	QWidget *widget = 0;
	switch ( setting ) {
		case FilterConfigurationSetting: {
			CheckCombobox *filterConfiguration = new CheckCombobox( parent );
            filterConfiguration->setMultipleSelectionOptions( CheckCombobox::ShowStringList );
			filterConfiguration->setToolTip( i18nc("@info:tooltip", 
					"The filter configuration(s) to be used with this stop(s)"));
			filterConfiguration->setWhatsThis( i18nc("@info:whatsthis", 
					"<para>Each stop can use a different set of filter configurations. "
                    "Choose these filter configurations here.\n"
					"<note>To create/edit/remove filter configurations use the filter page "
					"in the settings dialog.</note></para>") );
			widget = filterConfiguration;
			break;
		}
		case AlarmTimeSetting: {
			QSpinBox *alarmTime;
			alarmTime = new QSpinBox( parent );
			alarmTime->setMinimumSize( QSize(185, 0) );
			alarmTime->setMaximum( 255 );
			alarmTime->setValue( 5 );
			alarmTime->setSpecialValueText( i18nc("@info/plain", "On depart") );
        	alarmTime->setSuffix( i18nc("@info/plain", " minutes before departure") );
			widget = alarmTime;
			break;
		}
		case FirstDepartureConfigModeSetting: {
			QWidget *firstDeparture = new QWidget( parent );
			QVBoxLayout *layout = new QVBoxLayout( firstDeparture );
			
			QFormLayout *timeOffsetLayout = new QFormLayout();
			timeOffsetLayout->setContentsMargins( 0, 0, 0, 0 );
			QRadioButton *timeOffsetRadio = new QRadioButton(
					textForSetting(TimeOffsetOfFirstDepartureSetting), parent );
			timeOffsetRadio->setObjectName( QLatin1String("radio_") +
					nameForSetting(TimeOffsetOfFirstDepartureSetting) );
			QWidget *timeOffsetWidget = widgetWithNameForSetting(
					TimeOffsetOfFirstDepartureSetting, firstDeparture );
			QObject::connect( timeOffsetRadio, SIGNAL(toggled(bool)), 
							  timeOffsetWidget, SLOT(setEnabled(bool)) );
			timeOffsetLayout->addRow( timeOffsetRadio, timeOffsetWidget );
			
			QFormLayout *timeCustomLayout = new QFormLayout();
			timeCustomLayout->setContentsMargins( 0, 0, 0, 0 );
			QRadioButton *timeCustomRadio = new QRadioButton(
					textForSetting(TimeOfFirstDepartureSetting), parent );
			timeCustomRadio->setObjectName( QLatin1String("radio_") + 
					nameForSetting(TimeOfFirstDepartureSetting) );
			QWidget *timeCustomWidget = widgetWithNameForSetting(
					TimeOfFirstDepartureSetting, firstDeparture );
			QObject::connect( timeCustomRadio, SIGNAL(toggled(bool)), 
							  timeCustomWidget, SLOT(setEnabled(bool)) );
			timeCustomLayout->addRow( timeCustomRadio, timeCustomWidget );
			
			layout->addLayout( timeOffsetLayout );
			layout->addLayout( timeCustomLayout );
			widget = firstDeparture;
			break;
		}
		case TimeOffsetOfFirstDepartureSetting: {
			QSpinBox *timeOffset;
			timeOffset = new QSpinBox( parent );
			timeOffset->setWhatsThis( i18nc("@info:whatsthis", 
					"Here you can set the starting time of the departure list. "
					"No earlier departures will be shown.") );
			timeOffset->setSpecialValueText( i18nc("@info/plain", "Now") );
			timeOffset->setSuffix( i18nc("@info/plain", " minutes") );
			timeOffset->setPrefix( i18nc("@info/plain", "Now + ") );
			widget = timeOffset;
			break;
		}
		case TimeOfFirstDepartureSetting: {
			QTimeEdit *timeCustom;
			timeCustom = new QTimeEdit( parent );
			timeCustom->setEnabled( false );
			timeCustom->setTime( QTime(12, 0, 0) );
			widget = timeCustom;
			break;
		}
		default:
			if ( setting >= UserSetting ) {
				kDebug() << "No widget defined for custom setting" << static_cast<StopSetting>(setting);
			} else {
				kDebug() << "Intern error: No widget defined for setting" << static_cast<StopSetting>(setting);
			}
			break;
	}

	return widget;
}

QWidget* StopSettingsWidgetFactory::widgetWithNameForSetting( int setting, QWidget* parent ) const
{
	QWidget *widget = widgetForSetting( setting, parent );
	widget->setObjectName( nameForSetting(setting) );
	return widget;
}

QDebug& operator<<(QDebug debug, Stop stop)
{
	return debug << "StopName(" << stop.name << "," << stop.id << ")";
}

QDebug& operator<<(QDebug debug, StopList stopList)
{
	debug << "StopNameList (";
	foreach ( const Stop &stop, stopList ) {
		debug << stop;
	}
	return debug << ")";
}

QDebug& operator<<( QDebug debug, StopSetting setting )
{
	switch ( setting ) {
		case NoSetting:
			return debug << "NoSetting";
		case LocationSetting:
			return debug << "LocationSetting";
		case ServiceProviderSetting:
			return debug << "ServiceProviderSetting";
		case CitySetting:
			return debug << "CitySetting";
		case StopNameSetting:
			return debug << "StopNameSetting";
		case FilterConfigurationSetting:	
			return debug << "FilterConfigurationSetting";
		case AlarmTimeSetting:
			return debug << "AlarmTimeSetting";
		case FirstDepartureConfigModeSetting:
			return debug << "FirstDepartureConfigModeSetting";
		case TimeOffsetOfFirstDepartureSetting:
			return debug << "TimeOffsetOfFirstDepartureSetting";
		case TimeOfFirstDepartureSetting:
			return debug << "TimeOfFirstDepartureSetting";
		case UserSetting:
			return debug << "UserSetting";
		
		default:
			if ( setting > UserSetting ) {
				return debug << "UserSetting +" << (setting - static_cast<int>(UserSetting));
			} else {
				return debug << "Setting unknown" << setting;
			}
	}
}

}; // namespace Timetable
