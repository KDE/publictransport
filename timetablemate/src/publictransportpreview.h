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

#ifndef PUBLICTRANSPORTPREVIEW_HEADER
#define PUBLICTRANSPORTPREVIEW_HEADER

#include <QGraphicsView>
#include <Plasma/Corona>

class KPushButton;
namespace Plasma {
    class Applet;
    class Containment;
};

class PublicTransportPreview : public QGraphicsView {
    Q_OBJECT

public:
    PublicTransportPreview( QWidget *parent = 0 );

    bool isPlasmaPreviewShown() const { return m_applet; };
    void setSettings( const QString &serviceProviderID, const QString &stopName );

signals:
    void plasmaPreviewLoaded();

public slots:
    bool loadPlasmaPreview();
    void closePlasmaPreview();

protected:
    virtual void resizeEvent( QResizeEvent* event );

    private:
    void loadNoPlasmaScene();

    Plasma::Corona m_corona;
    Plasma::Containment *m_containment;
    Plasma::Applet *m_applet;
};

#endif // Multiple inclusion guard
