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

#include "plasmapreview.h"

#include <Plasma/Corona>
#include <Plasma/DataEngine>
#include <KPushButton>
#include <KMessageBox>

#include <QMetaClassInfo>
#include <QGraphicsProxyWidget>
#include <QGraphicsLinearLayout>
#include <KInputDialog>

PlasmaPreview::PlasmaPreview( QWidget *parent )
    : QGraphicsView( parent ), m_containment( 0 ), m_applet( 0 )
{
    setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    loadPlasmaPreview();
}

PlasmaPreview::~PlasmaPreview()
{
    if( isPlasmaPreviewShown() ) {
        // Remove applet
        m_containment->clearApplets();
        delete m_applet;

        // Remove containment
        delete m_containment;
    }
}

bool PlasmaPreview::loadPlasmaPreview()
{
    if( isPlasmaPreviewShown() ) {
        return true;
    }

    // Add the desktop containment
    m_containment = m_corona.addContainment( "desktop" );
    if( !m_containment ) {
        KMessageBox::information( this, i18nc("@info", "The plasma desktop containment could not "
                                              "be added. Ensure that you have plasma installed.") );
        return false;
    }
    QGraphicsScene *oldScene = scene();
    setScene( m_containment->scene() );
    setSceneRect( m_containment->geometry() );
    oldScene->deleteLater();

    // Add the PublicTransport applet
    m_applet = m_containment->addApplet( "publictransport" );
    if( !m_applet ) {
        delete m_containment;
        m_containment = 0;
        KMessageBox::information( this, i18nc("@info", "The PublicTransport applet could not be "
                                              "added. Ensure that you have it installed.") );
        return false;
    }
    m_applet->setFlag( QGraphicsItem::ItemIsMovable, false );
    setAlignment( Qt::AlignLeft | Qt::AlignTop );
    return true;
}

void PlasmaPreview::setSettings( const QString &serviceProviderID, const QString &stopName )
{
    if( !m_applet )
        return;

    // Set settings of the PublicTransport applet using a specific slot
    int index = m_applet->metaObject()->indexOfSlot( "setSettings(QString,QString)" );
    if( index == -1 ) {
        kDebug() << "Couldn't find slot with signarture setSettings(QString,QString) "
                 "in the publicTransport applet.";
        return;
    }

    bool success = m_applet->metaObject()->method( index ).invoke( m_applet,
                   Q_ARG( QString, serviceProviderID ), Q_ARG( QString, stopName ) );
    if( !success ) {
        kDebug() << "A call to setSettings in the publicTransport applet wasn't successful.";
    }

    if( stopName.isEmpty() ) {
        m_applet->showConfigurationInterface();
    }
}

void PlasmaPreview::resizeEvent( QResizeEvent *event )
{
    setUpdatesEnabled( false );
    QGraphicsView::resizeEvent( event );

    if( m_containment ) {
        m_containment->setMaximumSize( QWIDGETSIZE_MAX, QWIDGETSIZE_MAX );
        m_containment->setMinimumSize( size() );
        m_containment->setMaximumSize( size() );
        m_containment->resize( size() );
    }
    setUpdatesEnabled( true );
}

// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
