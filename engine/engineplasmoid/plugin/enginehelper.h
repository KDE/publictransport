#ifndef ENGINEHELPER_H
#define ENGINEHELPER_H

#include <QObject>
#include <QHash>
#include <QVariantMap>

class EngineHelper : public QObject
{
    Q_OBJECT

public:
    EngineHelper(QObject *parent=0);
    ~EngineHelper();

    Q_INVOKABLE bool isArrivalSource();
    Q_INVOKABLE bool isDepartureSource();
};

#endif // ENGINEHELPER_H