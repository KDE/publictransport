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

// Header
#include "global.h"

// KDE includes
#include <KDebug>
#include <KIcon>
#include <KIconEffect>
#include <KIconLoader>

// Qt includes
#include <QPainter>
#include <qmath.h>

// Plasma includes
#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    #include <Plasma/Animator>
    #include <Plasma/Animation>
#endif

KIcon GlobalApplet::stopIcon( RouteStopFlags routeStopFlags )
{
    if ( routeStopFlags.testFlag(RouteStopIsHighlighted) ) {
        return KIcon("flag-blue");
    } else if ( routeStopFlags.testFlag(RouteStopIsHomeStop) ) {
        return KIcon("go-home");
    } else if ( routeStopFlags.testFlag(RouteStopIsOrigin) ) {
        return KIcon("flag-red");
    } else if ( routeStopFlags.testFlag(RouteStopIsTarget) ) {
        return KIcon("flag-green");
    } else {
        return KIcon("public-transport-stop");
    }
}

KIcon GlobalApplet::makeOverlayIcon( const KIcon &icon, const KIcon &overlayIcon,
                                     const QSize &overlaySize, int iconExtend )
{
    QPixmap pixmap = icon.pixmap( iconExtend ), pixmapOverlay = overlayIcon.pixmap( overlaySize );
    QPainter p( &pixmap );
    p.drawPixmap( QPoint( iconExtend - overlaySize.width(), iconExtend - overlaySize.height() ), pixmapOverlay );
    p.end();
    KIcon resultIcon = KIcon();
    resultIcon.addPixmap( pixmap, QIcon::Normal );

    KIconEffect iconEffect;
    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::ActiveState );
    resultIcon.addPixmap( pixmap, QIcon::Selected );
    resultIcon.addPixmap( pixmap, QIcon::Active );

    return resultIcon;
}

KIcon GlobalApplet::makeOverlayIcon( const KIcon &icon, const QString &overlayIconName,
                                     const QSize &overlaySize, int iconExtend )
{
    return makeOverlayIcon( icon, KIcon( overlayIconName ), overlaySize, iconExtend );
}

KIcon GlobalApplet::makeOverlayIcon( const KIcon& icon, const QList<KIcon> &overlayIconsBottom,
                                     const QSize& overlaySize, int iconExtend )
{
    Q_ASSERT( !icon.isNull() );

    QPixmap pixmap = icon.pixmap( iconExtend );
    if ( pixmap.isNull() ) {
        kDebug() << "pixmap is Null";
        return icon;
    }

    QPainter p( &pixmap );
    int x = 0, xStep = iconExtend / overlayIconsBottom.count();
    foreach( const KIcon &overlayIcon, overlayIconsBottom ) {
        p.drawPixmap( QPoint( x, iconExtend - overlaySize.height() ), overlayIcon.pixmap( overlaySize ) );
        x += xStep;
    }
    p.end();
    KIcon resultIcon = KIcon();
    resultIcon.addPixmap( pixmap, QIcon::Normal );

    KIconEffect iconEffect;
    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::ActiveState );
    resultIcon.addPixmap( pixmap, QIcon::Selected );
    resultIcon.addPixmap( pixmap, QIcon::Active );

    return resultIcon;
}

#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
void GlobalApplet::startFadeAnimation( QGraphicsWidget* w, qreal targetOpacity )
{
    Plasma::Animation *anim = GlobalApplet::fadeAnimation( w, targetOpacity );
    if ( anim ) {
        anim->start( QAbstractAnimation::DeleteWhenStopped );
    }
}

Plasma::Animation* GlobalApplet::fadeAnimation( QGraphicsWidget* w, qreal targetOpacity )
{
    if ( w->geometry().width() * w->geometry().height() > 250000 ) {
        // Don't fade big widgets for performance reasons
        w->setOpacity( targetOpacity );
        return NULL;
    }

    Plasma::Animation *anim = Plasma::Animator::create( Plasma::Animator::FadeAnimation );
    anim->setTargetWidget( w );
    anim->setProperty( "startOpacity", w->opacity() );
    anim->setProperty( "targetOpacity", targetOpacity );
    return anim;
}
#endif
