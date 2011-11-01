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

#ifndef STOPSETTINGS_HEADER
#define STOPSETTINGS_HEADER

/** @file
* @brief Contains classes to store stop settings (Stop, StopList, StopSettings, StopSettingsList)
*   and a factory to create widgets for settings (StopSettingsWidgetFactory).
*
* @author Friedrich Pülz <fpuelz@gmx.de> */

#include "publictransporthelper_export.h"

#include "enums.h"

#include <QSharedPointer>
#include <QStringList>
#include <QDebug>
#include <QVariant> // For get() template functions
#include <QMetaType> // For Q_DECLARE_METATYPE

template<class Key, class T >
class QHash;

class QWidget;

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

class StopSettingsPrivate;

/**
 * @brief Stores information to identify a specific stop/station.
 *
 * The inforamtion consists of a user readable name and an ID, if available. Some providers use
 * IDs for stops, which should be used preferably when requesting data from the publictransport
 * data engine, to avoid ambiguities.
 *
 * To automatically get the ID if it's available and the stop name otherwise, use @ref nameOrId.
 *
 * A string (const char*, QString or QLatin1String) can be used instead of this class, eg. in
 * arguments with type Stop. The Stop object is then created automatically by the use of one of the
 * conversion constructors.
 **/
struct PUBLICTRANSPORTHELPER_EXPORT Stop {
public:
    /** @brief Constructor. */
    Stop();

    /**
     * @brief Constructor for conversion from <em>const char*</em>.
     *
     * The string @p name is used as name, while the id is left empty.
     **/
    Stop( const char *name );

    /**
     * @brief Constructor for conversion from QLatin1String.
     *
     * The string @p name is used as name, while the id is left empty.
     **/
    Stop( const QLatin1String &name );

    /**
     * @brief Creates a new Stop object with the given stop @p name and @p id.
     *
     * Can also be used for conversion from QString, where @p name is used as stop name, while
     * the id is left empty.
     *
     * @param name The name of the stop, which can be displayed to the user or given to the service
     *   provider. Default is QString().
     * @param id The ID of the stop, which should be used preferably when requesting data from the
     *   publictransport data engine. Default is QString().
     **/
    Stop( const QString &name, const QString &id = QString() );

    /** @brief Copy constructor. */
    Stop( const Stop &other );

    /** @brief Destructor. */
    ~Stop();

    /** @brief Sets all values to the ones of @p other. */
    Stop& operator=( const Stop &other );

    /** @brief Compares all values with the ones of @p other. */
    bool operator ==( const Stop &other ) const;

    /**
     * @brief Gets the QString to preferably use when requesting data from the publictransport
     *   data engine.
     *
     * @return The ID of the stop, if it's available. Otherwise the name of the stop.
     **/
    inline QString nameOrId() const {
        return id.isEmpty() ? name : id;
    };

    operator QString() const {
        return name;
    };

    /**
     * @brief The name of the stop.
     *
     * @note When requesting data from the publictransport data engine the ID of the stop should
     *   be used instead of the name, if available. You can simply use @ref nameOrId to get the
     *   best value.
     **/
    QString name;

    /**
     * @brief The ID of the stop.
     *
     * Can be empty if no ID is available for the stop.
     **/
    QString id;
};
typedef QList<Stop> StopList;

/**
 * @brief Stores settings for one set of stops/stations.
 *
 * May contain multiple stops that should be combined into one, eg. for stops that have different
 * names at the service provider but are very close to another.
 *
 * StopSettings can store values for settings in @p StopSetting and custom settings
 * (>= @p UserSetting) using @ref set. Settings are stored in a QHash, keys are @ref StopSetting.
 * To get values of settings use @ref get or the []-operator.
 *
 * @ref StopSettingsDialog, @ref StopWidget and/or @ref StopListWidget can be used to let the user
 * select stop settings. @ref StopSettingsDialog can also read/write custom settings, when a
 * custom StopSettingsWidgetFactory is used.
 *
 * The data of StopSettings objects is implicitly shared.
 **/
class PUBLICTRANSPORTHELPER_EXPORT StopSettings {
public:
    /**
     * @brief How stop IDs should be used.
     **/
    enum StopIdUsage {
        UseStopIdIfAvailable = 0, /**< @brief Use the stop ID if it's available.
                * Otherwise use the stop name. */
        UseStopName = 1 /**< @brief Always use the stop name, even if an ID is available. */
    };

    /** @brief Constructor. */
    StopSettings();

    /** @brief Constructor for convertsion from QHash. */
    StopSettings(const QHash<int, QVariant> &data);

    /** @brief Copy constructor. */
    StopSettings(const StopSettings &other);

    /** @brief Destructor. */
    ~StopSettings();

    /**
     * @brief Returns the currently selected stops as @ref StopList, ie. a QList of @ref Stop objects.
     *
     * This is the same as @code get<StopList>(StopNameSetting) @endcode
     **/
    const StopList stopList() const;

    /** @brief Returns the selected stop at the given @p index as @ref Stop object. */
    const Stop stop( int index ) const;

    /** @brief Gets a QStringList of the selected stop names. IDs are used where available. */
    QStringList stops( StopIdUsage stopIdUsage = UseStopName ) const;

    /**
     * @brief The IDs of the currently selected stops.
     *
     * Can contain empty strings if an ID isn't available.
     **/
    QStringList stopIDs() const;

    /**
     * @brief Checks if the given @p setting is set.
     *
     * @param setting The @ref StopSetting to check for. Should not be one of the default
     *   settings (LocationSetting, ServcieProviderSetting, CitySetting or StopNameSetting).
     *
     * @return True, if @p setting is set. False, otherwise.
     * @see setting
     **/
    bool hasSetting( int setting ) const;

    /**
     * @brief Overloaded version of @ref hasSetting(int) to also accept
     *   @ref StopSetting as argument type.
     **/
    inline bool hasSetting( StopSetting setting ) const {
        return hasSetting( static_cast<int>(setting) );
    };

    /**
     * @brief Gets a list of all used settings.
     *
     * @see @ref StopSetting
     **/
    QList<int> usedSettings() const;

    /**
     * @brief Gets a QHash with all used settings (@ref StopSetting) and it's values.
     *
     * The returned QHash can be used instead of the @ref StopSettings object, it contains the same
     * information. But then you can't use the helper function in this class, like @ref stops,
     * @ref setStops or @ref stopIDs.
     *
     * @see @ref StopSetting
     **/
    QHash<int, QVariant> settings() const;

    /**
     * @brief Gets the value of the given @p setting casted from QVariant to the given @p Type.
     *
     * You can also use the operator[] to get the value of a setting as QVariant.
     *
     * @param setting The @ref StopSetting to get the value of.
     *
     * @return The value of the given @p setting. If the setting isn't set (ie. @ref hasSetting
     *   returns false) a default-constructed value is returned.
     *
     * @see hasSetting
     * @see set
     * @see QVariant::isValid
     **/
    template< class Type >
    inline Type get( int setting ) const {
        return operator[]( setting ).value<Type>();
    };

    /**
     * @brief Overloaded version of @ref get(int) to also accept
     *   @ref StopSetting as argument type.
     **/
    template< class Type >
    inline Type get( StopSetting setting ) const {
        return get<Type>( static_cast<int>(setting) );
    };

    /**
     * @brief Sets the value of the given @p setting to @p value.
     *
     * @param setting The @ref StopSetting, which value should be set/changed.
     *
     * @param value The value for @p setting.
     *
     * @see clearSetting
     * @see get
     **/
    void set( int setting, const QVariant &value );

    /**
     * @brief Overloaded version of @ref set(int,QVariant) to also accept
     *   @ref StopSetting as argument type.
     **/
    inline void set( StopSetting setting, const QVariant &value ) {
        set( static_cast<int>(setting), value );
    };

    /**
     * @brief Clears the value of the given @p setting.
     *
     * @param setting The @ref StopSetting which value should be discarded.
     *
     * @see set
     * @see get
     **/
    void clearSetting( int setting );

    /**
     * @brief Sets the ID of the @p stop to @p id.
     *
     * @param stop The name of the stop, which ID should be changed.
     *
     * @param id The new ID for the stop.
     **/
    void setIdOfStop( const QString &stop, const QString &id );

    /** @brief Sets one currently selected stop.
     *
     * This function uses @ref setStops with a QStringList with only the given @p stop in it.
     */
    void setStop( const Stop &stop );

    /** @brief Sets the currently selected stops */
    void setStops( const QStringList &stops, const QStringList &stopIDs = QStringList() );

    /** @brief Sets the currently selected stops */
    void setStops( const StopList &stopList );

    const QVariant operator[]( int setting ) const;
    QVariant &operator[]( int setting );
    const QVariant operator[]( StopSetting setting ) const;
    QVariant &operator[]( StopSetting setting );

    /** @brief Simple assignment operator. */
    StopSettings &operator =( const StopSettings &rhs );

    /** @brief Simple equality operator. */
    bool operator ==( const StopSettings &other ) const;

protected:
    QSharedDataPointer<StopSettingsPrivate> d;
};

/** @brief A QList of StopSettings object with additional functions. */
class PUBLICTRANSPORTHELPER_EXPORT StopSettingsList : public QList<StopSettings> {
public:
    StopSettingsList() : QList<StopSettings>() {};

    int findStopSettings( const QString &stopName, int startIndex = 0 );
    void removeIntermediateSettings( int startIndex = 0,
                                     const QString &id = "-- Intermediate Stop --",
                                     int stopSetting = UserSetting + 100 );
};

/**
 * @brief A widget factory that creates widgets for given @ref StopSetting.
 *
 * Can be subclassed to add widgets for more settings.
 * All functions should be overwritten to return correct values for custom settings,
 * which should begin with @ref StopSetting::UserSetting. The default implementations should
 * be called for settings that aren't handled in the derived class.
 *
 * @note You need to overwrite the typedef @ref Pointer with a QSharedPointer around your custom
 * widget factory.
 *
 * Here is an example of a custom stop settings widget factory that adds a new setting which
 * handles integer values using a QSpinBox:
 * @code
 * class MyStopSettingsWidgetFactory : public StopSettingsWidgetFactory {
 * public:
 *   virtual QString textForSetting( int setting ) const
 *   {
 *     if ( setting == StopSetting::UserSetting ) {
 *       // Return the label text for the widget associated with StopSetting::UserSetting
 *       return "Test Setting:";
 *     } else {
 *       // Use default implementation
 *       return StopSettingsWidgetFactory::textForSetting( setting );
 *     }
 *   };
 *
 *   virtual QString nameForSetting( int setting ) const
 *   {
 *     if ( setting == StopSetting::UserSetting ) {
 *       // Return the object name to use for the custom setting widget
 *       return "TestSetting";
 *     } else {
 *       // Use default implementation
 *       return StopSettingsWidgetFactory::nameForSetting( setting );
 *     }
 *   };
 *
 *   virtual QVariant valueOfSetting( const QWidget* widget, int setting ) const
 *   {
 *     if ( setting == StopSettings::UserSetting ) {
 *       // Get the widgets value and return it
 *       return qobject_cast< QSpinBox* >( widget )->value();
 *     } else {
 *       // Use default implementation
 *       return StopSettingsWidgetFactory::valueOfSetting( widget, setting );
 *     }
 *   }
 *
 *   virtual void setValueOfSetting( QWidget* widget, int setting, const QVariant& value) const
 *   {
 *     if ( setting == StopSettings::UserSetting ) {
 *       // Set the widgets value
 *       qobject_cast< QSpinBox* >( widget )->setValue( value.toInt() );
 *     } else {
 *       // Use default implementation
 *       StopSettingsWidgetFactory::setValueOfSetting( widget, setting, value );
 *     }
 *   };
 *
 *   typedef QSharedPointer<MyStopSettingsWidgetFactory> Pointer;
 *
 * protected:
 *   virtual QWidget *widgetForSetting( int setting, QWidget *parent = 0 ) const
 *   {
 *     if ( setting == StopSetting::UserSetting ) {
 *       // Create the custom setting widget
 *       QSpinBox *w = new QSpinBox( parent );
 *       w->setSuffix( " suffix" );
 *       w->setMinimum( 50 );
 *       w->setMaximum( 500 );
 *       return w;
 *     } else {
 *       // Use default implementation
 *       return StopSettingsWidgetFactory::widgetForSetting( setting, parent );
 *     }
 *   };
 * };
 * @endcode
 *
 * @see StopSetting
 **/
class PUBLICTRANSPORTHELPER_EXPORT StopSettingsWidgetFactory {
public:
    /** @brief Destructor. */
    virtual ~StopSettingsWidgetFactory();

    /**
     * @brief Gets the object name to set on the widget that gets created
     *   with @ref widgetForSetting (QObject::setObjectName).
     *
     * This is used to find the widget associated with the given @p setting using
     * QObject::findChild.
     *
     * @note This function doesn't need to be overloaded, but it can make it easier to debug,
     *   if the widget has a meaningful object name.
     *   The default implementation automatically creates a name for custom settings "UserSetting_X"
     *   where X is an integer (the setting value, ie. @ref StopSetting::UserSetting).
     *
     * @param setting The @ref StopSetting to get the object name for.
     *
     * @return The object name for the widget associated with the given @p setting.
     * @see StopSetting
     **/
    virtual QString nameForSetting( int setting ) const;

    /**
     * @brief Returns whether or not the widget for the given @p setting is a detailed setting.
     *
     * Detailed settings are for example added to the detailed section of a @ref StopSettingsDialog,
     * while others are added to the main widget of the dialog.
     *
     * The default implementation returns true if @p setting is @ref UserSetting or higher.
     * That means that by default custom widgets are added into the detailed section of a
     * @ref StopSettingsDialog.
     *
     * @note LocationSetting, ServiceProviderSetting, CitySetting and StopNameSetting may
     *   <em>not</em> be detailed settings (because they aren't created in the factory). That means
     *   that you shouldn't handle these settings in an overwritten function: Instead simply call
     *   the default implementation, which returns false for these settings.
     *
     * @param setting The @ref StopSetting to check.
     *
     * @return True, if @p setting is less important (eg. should be shown in a dialogs details
     *   section). False, otherwise.
     * @see StopSetting
     **/
    virtual bool isDetailsSetting( int setting ) const;

    /**
     * @brief Gets the text to use as label for the widget that gets created with widgetForSetting.
     *
     * @param setting The @ref StopSetting to get the label text for.
     *
     * @return The label text for the widget associated with the given @p setting.
     * @see StopSetting
     **/
    virtual QString textForSetting( int setting ) const;

    /**
     * @brief Calls widgetForSetting and sets the object name of the returned widget.
     *
     * The object name of the returned widget gets set to the one returned by nameForSetting.
     *
     * @param setting The setting to get the widget for.
     * @param parent The parent for the new widget. Default is 0.
     *
     * @return QWidget* The created widget for the given @p setting.
     **/
    QWidget *widgetWithNameForSetting( int setting, QWidget *parent = 0 ) const;

    /**
     * @brief Get the value of the @p widget associated with the given @p setting.
     *
     * @param widget The widget associated with the given @p setting.
     * @param setting The @ref StopSetting to get the value for.
     * @param stopIndex The index of the stop the setting is for or -1 if not in a list of
     *   stop settings. Currently used for FilterConfigurationSetting, to get a list of activated
     *   filter configurations for the stop from a list of affected stops per filter configuration.
     *
     * @return The value for the given @p setting.
     * @see StopSetting
     **/
    virtual QVariant valueOfSetting( const QWidget *widget, int setting, int stopIndex = -1 ) const;

    /**
     * @brief Sets the value of the @p widget associated with the given @p setting
     *   to @p value.
     *
     * @param widget The widget associated with the given @p setting.
     * @param setting The @ref StopSetting to set the value for.
     * @param value The new value for the given @p setting.
     *
     * @see StopSetting
     **/
    virtual void setValueOfSetting( QWidget *widget, int setting, const QVariant &value ) const;

    /**
     * @brief A typedef for a QSharedPointer.
     *
     * Used instead of normal C++ pointers, eg. to share the factory between
     * @ref StopWidget and @ref StopSettingsDialog (StopWidget holds a Pointer to the factory and
     * shares it with stop settings dialogs, but the factory shouldn't be deleted once a
     * StopSettingsDialog gets closed).
     *
     * @note You need to overwrite this with a QSharedPointer around your custom widget factory.
     **/
    typedef QSharedPointer<StopSettingsWidgetFactory> Pointer;

protected:
    /**
     * @brief Creates and returns the widget associated with the given @p setting.
     *
     * @param setting The @ref StopSetting to create a widget for.
     *
     * @return A new instance of the widget associated with the given @p setting.
     * @see StopSetting
     **/
    virtual QWidget *widgetForSetting( int setting, QWidget *parent = 0 ) const;
};

QDebug &operator <<(QDebug debug, Stop stop);
QDebug &operator <<(QDebug debug, StopList stopList);
QDebug &operator <<(QDebug debug, StopSetting setting);

}; // namespace Timetable

Q_DECLARE_METATYPE( Timetable::Stop );
Q_DECLARE_METATYPE( Timetable::StopList );
Q_DECLARE_METATYPE( Timetable::StopSettings );
Q_DECLARE_METATYPE( Timetable::StopSettingsList );

#endif // Multiple inclusion guard
