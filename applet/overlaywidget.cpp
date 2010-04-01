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

#include "overlaywidget.h"

#include "global.h"

#include <Plasma/Applet>
#include <Plasma/PaintUtils>
#include <KGlobalSettings>

#include <QPainter>
#include <QGraphicsScene>

#if QT_VERSION >= 0x040600
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QGraphicsEffect>
#endif
#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
#include <Plasma/Animation>
#endif

OverlayWidget::OverlayWidget( QGraphicsWidget* parent, QGraphicsWidget* under )
	    : QGraphicsWidget( parent ), opacity( 0.4 )
#if QT_VERSION >= 0x040600
	    , m_under( 0 ), m_blur( 0 )
#endif
	    {
    resize( parent->size() );
    setZValue( 10000 );
    m_under = under;
    under->setEnabled( false );

#if QT_VERSION >= 0x040600
    if ( under && KGlobalSettings::graphicEffectsLevel() != KGlobalSettings::NoEffects )
    {
	m_blur = new QGraphicsBlurEffect( this );
	under->setGraphicsEffect( m_blur );
	if ( under->geometry().width() * under->geometry().height() <= 250000 ) {
	    m_blur->setBlurHints( QGraphicsBlurEffect::AnimationHint );
	    QPropertyAnimation *blurAnim = new QPropertyAnimation( m_blur, "blurRadius" );
	    blurAnim->setStartValue( 0 );
	    blurAnim->setEndValue( 5 );
	    blurAnim->setDuration( 1000 );
	    blurAnim->start( QAbstractAnimation::DeleteWhenStopped );
	}
    } else // widget is too big
	m_blur->setBlurHints( QGraphicsBlurEffect::PerformanceHint );
#endif
}

void OverlayWidget::destroy() {
#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    if ( m_under->geometry().width() * m_under->geometry().height() <= 250000 ) {
	Plasma::Animation *fadeAnim = Global::fadeAnimation( this, 0 );

	QParallelAnimationGroup *parGroup = new QParallelAnimationGroup;
	connect( parGroup, SIGNAL(finished()), this, SLOT(overlayAnimationComplete()) );
	if ( fadeAnim )
	    parGroup->addAnimation( fadeAnim );
	if ( m_blur ) {
	    QPropertyAnimation *blurAnim = new QPropertyAnimation( m_blur, "blurRadius" );
	    blurAnim->setStartValue( m_blur->blurRadius() );
	    blurAnim->setEndValue( 0 );
	    parGroup->addAnimation( blurAnim );
	}
	parGroup->start( QAbstractAnimation::DeleteWhenStopped );

	m_under->setEnabled( true );
    } else // widget is too big
	overlayAnimationComplete();
#else
    overlayAnimationComplete();
#endif
}

void OverlayWidget::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
			   QWidget* widget ) {
    Q_UNUSED( option )
    Q_UNUSED( widget )

    if ( qFuzzyCompare(1, 1 + opacity) )
	return;

    QColor wash = Plasma::Theme::defaultTheme()->color( Plasma::Theme::BackgroundColor );
    wash.setAlphaF( opacity );

    Plasma::Applet *applet = qobject_cast<Plasma::Applet *>(parentWidget());
    QPainterPath backgroundShape;
    if ( !applet || applet->backgroundHints() & Plasma::Applet::StandardBackground ) {
	// FIXME: a resize here is nasty, but perhaps still better than an eventfilter just for that..
	if ( parentWidget()->contentsRect().size() != size() )
	    resize( parentWidget()->contentsRect().size() );

	backgroundShape = Plasma::PaintUtils::roundedRectangle(contentsRect(), 5);
    } else {
	backgroundShape = shape();
    }

    painter->setRenderHints( QPainter::Antialiasing );
    painter->fillPath( backgroundShape, wash );
}

void OverlayWidget::overlayAnimationComplete() {
    if ( scene() )
	scene()->removeItem( this );
    deleteLater();

    m_under->setEnabled( true );
    #if QT_VERSION >= 0x040600
    m_under->setGraphicsEffect( NULL );
    #endif
}