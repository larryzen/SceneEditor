/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/
#include "PreHeader.h"
#include "qwt_scale_draw.h"
#include "qwt_scale_div.h"
#include "qwt_scale_map.h"
#include "qwt_math.h"
#include "qwt_painter.h"
#include <qpen.h>
#include <qpainter.h>
#include <qmath.h>

#if QT_VERSION < 0x040601
#define qFastSin(x) qSin(x)
#define qFastCos(x) qCos(x)
#endif

class QwtScaleDraw::PrivateData
{
public:
    PrivateData():
        len( 0 ),
        alignment( QwtScaleDraw::BottomScale ),
        labelAlignment( 0 ),
        labelRotation( 0.0 )
    {
    }

    QPointF pos;
    double len;

    Alignment alignment;

    Qt::Alignment labelAlignment;
    double labelRotation;
};

/*!
  \brief Constructor

  The range of the scale is initialized to [0, 100],
  The position is at (0, 0) with a length of 100.
  The orientation is QwtAbstractScaleDraw::Bottom.
*/
QwtScaleDraw::QwtScaleDraw()
{
    d_data = new QwtScaleDraw::PrivateData;
    setLength( 100 );
}

//! Destructor
QwtScaleDraw::~QwtScaleDraw()
{
    delete d_data;
}

/*!
   Return alignment of the scale
   \sa setAlignment()
   \return Alignment of the scale
*/
QwtScaleDraw::Alignment QwtScaleDraw::alignment() const
{
    return d_data->alignment;
}

/*!
   Set the alignment of the scale

   \param align Alignment of the scale

   The default alignment is QwtScaleDraw::BottomScale
   \sa alignment()
*/
void QwtScaleDraw::setAlignment( Alignment align )
{
    d_data->alignment = align;
}

/*!
  Return the orientation

  TopScale, BottomScale are horizontal (Qt::Horizontal) scales,
  LeftScale, RightScale are vertical (Qt::Vertical) scales.

  \return Orientation of the scale

  \sa alignment()
*/
Qt::Orientation QwtScaleDraw::orientation() const
{
    switch ( d_data->alignment )
    {
        case TopScale:
        case BottomScale:
            return Qt::Horizontal;
        case LeftScale:
        case RightScale:
        default:
            return Qt::Vertical;
    }
}

/*!
  \brief Determine the minimum border distance

  This member function returns the minimum space
  needed to draw the mark labels at the scale's endpoints.

  \param font Font
  \param start Start border distance
  \param end End border distance
*/
void QwtScaleDraw::getBorderDistHint( 
    const QFont &font, int &start, int &end ) const
{
    start = 0;
    end = 0;

    if ( !hasComponent( QwtAbstractScaleDraw::Labels ) )
        return;

    const QList<double> &ticks = scaleDiv().ticks( QwtScaleDiv::MajorTick );
    if ( ticks.count() == 0 )
        return;

    // Find the ticks, that are mapped to the borders.
    // minTick is the tick, that is mapped to the top/left-most position
    // in widget coordinates.

    double minTick = ticks[0];
    double minPos = scaleMap().transform( minTick );
    double maxTick = minTick;
    double maxPos = minPos;

    for ( int i = 1; i < ticks.count(); i++ )
    {
        const double tickPos = scaleMap().transform( ticks[i] );
        if ( tickPos < minPos )
        {
            minTick = ticks[i];
            minPos = tickPos;
        }
        if ( tickPos > scaleMap().transform( maxTick ) )
        {
            maxTick = ticks[i];
            maxPos = tickPos;
        }
    }

    double e = 0.0;
    double s = 0.0;
    if ( orientation() == Qt::Vertical )
    {
        s = -labelRect( font, minTick ).top();
        s -= qAbs( minPos - qRound( scaleMap().p2() ) );

        e = labelRect( font, maxTick ).bottom();
        e -= qAbs( maxPos - scaleMap().p1() );
    }
    else
    {
        s = -labelRect( font, minTick ).left();
        s -= qAbs( minPos - scaleMap().p1() );

        e = labelRect( font, maxTick ).right();
        e -= qAbs( maxPos - scaleMap().p2() );
    }

    if ( s < 0.0 )
        s = 0.0;
    if ( e < 0.0 )
        e = 0.0;

    start = qCeil( s );
    end = qCeil( e );
}

/*!
  Determine the minimum distance between two labels, that is necessary
  that the texts don't overlap.

  \param font Font
  \return The maximum width of a label

  \sa getBorderDistHint()
*/

int QwtScaleDraw::minLabelDist( const QFont &font ) const
{
    if ( !hasComponent( QwtAbstractScaleDraw::Labels ) )
        return 0;

    const QList<double> &ticks = scaleDiv().ticks( QwtScaleDiv::MajorTick );
    if ( ticks.isEmpty() )
        return 0;

    const QFontMetrics fm( font );

    const bool vertical = ( orientation() == Qt::Vertical );

    QRectF bRect1;
    QRectF bRect2 = labelRect( font, ticks[0] );
    if ( vertical )
    {
        bRect2.setRect( -bRect2.bottom(), 0.0, bRect2.height(), bRect2.width() );
    }

    double maxDist = 0.0;

    for ( int i = 1; i < ticks.count(); i++ )
    {
        bRect1 = bRect2;
        bRect2 = labelRect( font, ticks[i] );
        if ( vertical )
        {
            bRect2.setRect( -bRect2.bottom(), 0.0,
                bRect2.height(), bRect2.width() );
        }

        double dist = fm.leading(); // space between the labels
        if ( bRect1.right() > 0 )
            dist += bRect1.right();
        if ( bRect2.left() < 0 )
            dist += -bRect2.left();

        if ( dist > maxDist )
            maxDist = dist;
    }

    double angle = qwtRadians( labelRotation() ); 
    if ( vertical )
        angle += M_PI / 2;

    const double sinA = qFastSin( angle ); // qreal -> double
    if ( qFuzzyCompare( sinA + 1.0, 1.0 ) )
        return qCeil( maxDist );

    const int fmHeight = fm.ascent() - 2;

    // The distance we need until there is
    // the height of the label font. This height is needed
    // for the neighbored label.

    double labelDist = fmHeight / qFastSin( angle ) * qFastCos( angle );
    if ( labelDist < 0 )
        labelDist = -labelDist;

    // For text orientations close to the scale orientation

    if ( labelDist > maxDist )
        labelDist = maxDist;

    // For text orientations close to the opposite of the
    // scale orientation

    if ( labelDist < fmHeight )
        labelDist = fmHeight;

    return qCeil( labelDist );
}

/*!
   Calculate the width/height that is needed for a
   vertical/horizontal scale.

   The extent is calculated from the pen width of the backbone,
   the major tick length, the spacing and the maximum width/height
   of the labels.

   \param font Font used for painting the labels
   \return Extent

   \sa minLength()
*/
double QwtScaleDraw::extent( const QFont &font ) const
{
    double d = 0;

    if ( hasComponent( QwtAbstractScaleDraw::Labels ) )
    {
        if ( orientation() == Qt::Vertical )
            d = maxLabelWidth( font );
        else
            d = maxLabelHeight( font );

        if ( d > 0 )
            d += spacing();
    }

    if ( hasComponent( QwtAbstractScaleDraw::Ticks ) )
    {
        d += maxTickLength();
    }

    if ( hasComponent( QwtAbstractScaleDraw::Backbone ) )
    {
        const double pw = qMax( 1, penWidth() );  // pen width can be zero
        d += pw;
    }

    d = qMax( d, minimumExtent() );
    return d;
}

/*!
   Calculate the minimum length that is needed to draw the scale

   \param font Font used for painting the labels
   \return Minimum length that is needed to draw the scale

   \sa extent()
*/
int QwtScaleDraw::minLength( const QFont &font ) const
{
    int startDist, endDist;
    getBorderDistHint( font, startDist, endDist );

    const QwtScaleDiv &sd = scaleDiv();

    const uint minorCount =
        sd.ticks( QwtScaleDiv::MinorTick ).count() +
        sd.ticks( QwtScaleDiv::MediumTick ).count();
    const uint majorCount =
        sd.ticks( QwtScaleDiv::MajorTick ).count();

    int lengthForLabels = 0;
    if ( hasComponent( QwtAbstractScaleDraw::Labels ) )
        lengthForLabels = minLabelDist( font ) * majorCount;

    int lengthForTicks = 0;
    if ( hasComponent( QwtAbstractScaleDraw::Ticks ) )
    {
        const double pw = qMax( 1, penWidth() );  // penwidth can be zero
        lengthForTicks = qCeil( ( majorCount + minorCount ) * ( pw + 1.0 ) );
    }

    return startDist + endDist + qMax( lengthForLabels, lengthForTicks );
}

/*!
   Find the position, where to paint a label

   The position has a distance that depends on the length of the ticks 
   in direction of the alignment().

   \param value Value
   \return Position, where to paint a label
*/
QPointF QwtScaleDraw::labelPosition( double value ) const
{
    const double tval = scaleMap().transform( value );
    double dist = spacing();
    if ( hasComponent( QwtAbstractScaleDraw::Backbone ) )
        dist += qMax( 1, penWidth() );

    if ( hasComponent( QwtAbstractScaleDraw::Ticks ) )
        dist += tickLength( QwtScaleDiv::MajorTick );

    double px = 0;
    double py = 0;

    switch ( alignment() )
    {
        case RightScale:
        {
            px = d_data->pos.x() + dist;
            py = tval;
            break;
        }
        case LeftScale:
        {
            px = d_data->pos.x() - dist;
            py = tval;
            break;
        }
        case BottomScale:
        {
            px = tval;
            py = d_data->pos.y() + dist;
            break;
        }
        case TopScale:
        {
            px = tval;
            py = d_data->pos.y() - dist;
            break;
        }
    }

    return QPointF( px, py );
}

/*!
   Draw a tick

   \param painter Painter
   \param value Value of the tick
   \param len Length of the tick

   \sa drawBackbone(), drawLabel()
*/
void QwtScaleDraw::drawTick( QPainter *painter, double value, double len ) const
{
    if ( len <= 0 )
        return;

    const bool roundingAlignment = QwtPainter::roundingAlignment( painter );

    QPointF pos = d_data->pos;

    double tval = scaleMap().transform( value );
    if ( roundingAlignment )
        tval = qRound( tval );

    const int pw = penWidth();
    int a = 0;
    if ( pw > 1 && roundingAlignment )
        a = 1;

    switch ( alignment() )
    {
        case LeftScale:
        {
            double x1 = pos.x() + a;
            double x2 = pos.x() + a - pw - len;
            if ( roundingAlignment )
            {
                x1 = qRound( x1 );
                x2 = qRound( x2 );
            }

            QwtPainter::drawLine( painter, x1, tval, x2, tval );
            break;
        }

        case RightScale:
        {
            double x1 = pos.x();
            double x2 = pos.x() + pw + len;
            if ( roundingAlignment )
            {
                x1 = qRound( x1 );
                x2 = qRound( x2 );
            }

            QwtPainter::drawLine( painter, x1, tval, x2, tval );
            break;
        }

        case BottomScale:
        {
            double y1 = pos.y();
            double y2 = pos.y() + pw + len;
            if ( roundingAlignment )
            {
                y1 = qRound( y1 );
                y2 = qRound( y2 );
            }

            QwtPainter::drawLine( painter, tval, y1, tval, y2 );
            break;
        }

        case TopScale:
        {
            double y1 = pos.y() + a;
            double y2 = pos.y() - pw - len + a;
            if ( roundingAlignment )
            {
                y1 = qRound( y1 );
                y2 = qRound( y2 );
            }

            QwtPainter::drawLine( painter, tval, y1, tval, y2 );
            break;
        }
    }
}

/*!
   Draws the baseline of the scale
   \param painter Painter

   \sa drawTick(), drawLabel()
*/
void QwtScaleDraw::drawBackbone( QPainter *painter ) const
{
    const bool doAlign = QwtPainter::roundingAlignment( painter );

    const QPointF &pos = d_data->pos;
    const double len = d_data->len;
    const int pw = qMax( penWidth(), 1 );

    // pos indicates a border not the center of the backbone line
    // so we need to shift its position depending on the pen width
    // and the alignment of the scale

    double off;
    if ( doAlign )
    {
        if ( alignment() == LeftScale || alignment() == TopScale )
            off = ( pw - 1 ) / 2;
        else
            off = pw / 2;
    }
    else
    {
        off = 0.5 * penWidth();
    }

    switch ( alignment() )
    {
        case LeftScale:
        {
            double x = pos.x() - off;
            if ( doAlign )
                x = qRound( x );

            QwtPainter::drawLine( painter, x, pos.y(), x, pos.y() + len );
            break;
        }
        case RightScale:
        {
            double x = pos.x() + off;
            if ( doAlign )
                x = qRound( x );

            QwtPainter::drawLine( painter, x, pos.y(), x, pos.y() + len );
            break;
        }
        case TopScale:
        {
            double y = pos.y() - off;
            if ( doAlign )
                y = qRound( y );

            QwtPainter::drawLine( painter, pos.x(), y, pos.x() + len, y );
            break;
        }
        case BottomScale:
        {
            double y = pos.y() + off;
            if ( doAlign )
                y = qRound( y );

            QwtPainter::drawLine( painter, pos.x(), y, pos.x() + len, y );
            break;
        }
    }
}

/*!
  \brief Move the position of the scale

  The meaning of the parameter pos depends on the alignment:
  <dl>
  <dt>QwtScaleDraw::LeftScale
  <dd>The origin is the topmost point of the
      backbone. The backbone is a vertical line.
      Scale marks and labels are drawn
      at the left of the backbone.
  <dt>QwtScaleDraw::RightScale
  <dd>The origin is the topmost point of the
      backbone. The backbone is a vertical line.
      Scale marks and labels are drawn
      at the right of the backbone.
  <dt>QwtScaleDraw::TopScale
  <dd>The origin is the leftmost point of the
      backbone. The backbone is a horizontal line.
      Scale marks and labels are drawn
      above the backbone.
  <dt>QwtScaleDraw::BottomScale
  <dd>The origin is the leftmost point of the
      backbone. The backbone is a horizontal line
      Scale marks and labels are drawn
      below the backbone.
  </dl>

  \param pos Origin of the scale

  \sa pos(), setLength()
*/
void QwtScaleDraw::move( const QPointF &pos )
{
    d_data->pos = pos;
    updateMap();
}

/*!
   \return Origin of the scale
   \sa move(), length()
*/
QPointF QwtScaleDraw::pos() const
{
    return d_data->pos;
}

/*!
  Set the length of the backbone.

  The length doesn't include the space needed for
  overlapping labels.

  \param length Length of the backbone

  \sa move(), minLabelDist()
*/
void QwtScaleDraw::setLength( double length )
{
#if 1
    if ( length >= 0 && length < 10 )
        length = 10;

    // why should we accept negative lengths ???
    if ( length < 0 && length > -10 )
        length = -10;
#else
    length = qMax( length, 10 );
#endif

    d_data->len = length;
    updateMap();
}

/*!
   \return the length of the backbone
   \sa setLength(), pos()
*/
double QwtScaleDraw::length() const
{
    return d_data->len;
}

/*!
   Draws the label for a major scale tick

   \param painter Painter
   \param value Value

   \sa drawTick(), drawBackbone(), boundingLabelRect()
*/
void QwtScaleDraw::drawLabel( QPainter *painter, double value ) const
{
    QwtText lbl = tickLabel( painter->font(), value );
    if ( lbl.isEmpty() )
        return;

    QPointF pos = labelPosition( value );

    QSizeF labelSize = lbl.textSize( painter->font() );

    const QTransform transform = labelTransformation( pos, labelSize );

    painter->save();
    painter->setWorldTransform( transform, true );

    lbl.draw ( painter, QRect( QPoint( 0, 0 ), labelSize.toSize() ) );

    painter->restore();
}

/*!
  \brief Find the bounding rectangle for the label.

  The coordinates of the rectangle are absolute ( calculated from pos() ).
  in direction of the tick.

  \param font Font used for painting
  \param value Value

  \return Bounding rectangle
  \sa labelRect()
*/
QRect QwtScaleDraw::boundingLabelRect( const QFont &font, double value ) const
{
    QwtText lbl = tickLabel( font, value );
    if ( lbl.isEmpty() )
        return QRect();

    const QPointF pos = labelPosition( value );
    QSizeF labelSize = lbl.textSize( font );

    const QTransform transform = labelTransformation( pos, labelSize );
    return transform.mapRect( QRect( QPoint( 0, 0 ), labelSize.toSize() ) );
}

/*!
   Calculate the transformation that is needed to paint a label
   depending on its alignment and rotation.

   \param pos Position where to paint the label
   \param size Size of the label

   \return Transformation matrix
   \sa setLabelAlignment(), setLabelRotation()
*/
QTransform QwtScaleDraw::labelTransformation(
    const QPointF &pos, const QSizeF &size ) const
{
    QTransform transform;
    transform.translate( pos.x(), pos.y() );
    transform.rotate( labelRotation() );

    int flags = labelAlignment();
    if ( flags == 0 )
    {
        switch ( alignment() )
        {
            case RightScale:
            {
                if ( flags == 0 )
                    flags = Qt::AlignRight | Qt::AlignVCenter;
                break;
            }
            case LeftScale:
            {
                if ( flags == 0 )
                    flags = Qt::AlignLeft | Qt::AlignVCenter;
                break;
            }
            case BottomScale:
            {
                if ( flags == 0 )
                    flags = Qt::AlignHCenter | Qt::AlignBottom;
                break;
            }
            case TopScale:
            {
                if ( flags == 0 )
                    flags = Qt::AlignHCenter | Qt::AlignTop;
                break;
            }
        }
    }

    double x, y;

    if ( flags & Qt::AlignLeft )
        x = -size.width();
    else if ( flags & Qt::AlignRight )
        x = 0.0;
    else // Qt::AlignHCenter
        x = -( 0.5 * size.width() );

    if ( flags & Qt::AlignTop )
        y = -size.height();
    else if ( flags & Qt::AlignBottom )
        y = 0;
    else // Qt::AlignVCenter
        y = -( 0.5 * size.height() );

    transform.translate( x, y );

    return transform;
}

/*!
  Find the bounding rectangle for the label. The coordinates of
  the rectangle are relative to spacing + tick length from the backbone
  in direction of the tick.

  \param font Font used for painting
  \param value Value

   \return Bounding rectangle that is needed to draw a label
*/
QRectF QwtScaleDraw::labelRect( const QFont &font, double value ) const
{
    QwtText lbl = tickLabel( font, value );
    if ( lbl.isEmpty() )
        return QRectF( 0.0, 0.0, 0.0, 0.0 );

    const QPointF pos = labelPosition( value );

    const QSizeF labelSize = lbl.textSize( font );
    const QTransform transform = labelTransformation( pos, labelSize );

    QRectF br = transform.mapRect( QRectF( QPointF( 0, 0 ), labelSize ) );
    br.translate( -pos.x(), -pos.y() );

    return br;
}

/*!
   Calculate the size that is needed to draw a label

   \param font Label font
   \param value Value

   \return Size that is needed to draw a label
*/
QSizeF QwtScaleDraw::labelSize( const QFont &font, double value ) const
{
    return labelRect( font, value ).size();
}

/*!
  Rotate all labels.

  When changing the rotation, it might be necessary to
  adjust the label flags too. Finding a useful combination is
  often the result of try and error.

  \param rotation Angle in degrees. When changing the label rotation,
                  the label flags often needs to be adjusted too.

  \sa setLabelAlignment(), labelRotation(), labelAlignment().

*/
void QwtScaleDraw::setLabelRotation( double rotation )
{
    d_data->labelRotation = rotation;
}

/*!
  \return the label rotation
  \sa setLabelRotation(), labelAlignment()
*/
double QwtScaleDraw::labelRotation() const
{
    return d_data->labelRotation;
}

/*!
  \brief Change the label flags

  Labels are aligned to the point tick length + spacing away from the backbone.

  The alignment is relative to the orientation of the label text.
  In case of an flags of 0 the label will be aligned
  depending on the orientation of the scale:

      QwtScaleDraw::TopScale: Qt::AlignHCenter | Qt::AlignTop\n
      QwtScaleDraw::BottomScale: Qt::AlignHCenter | Qt::AlignBottom\n
      QwtScaleDraw::LeftScale: Qt::AlignLeft | Qt::AlignVCenter\n
      QwtScaleDraw::RightScale: Qt::AlignRight | Qt::AlignVCenter\n

  Changing the alignment is often necessary for rotated labels.

  \param alignment Or'd Qt::AlignmentFlags see <qnamespace.h>

  \sa setLabelRotation(), labelRotation(), labelAlignment()
  \warning The various alignments might be confusing.
           The alignment of the label is not the alignment
           of the scale and is not the alignment of the flags
           ( QwtText::flags() ) returned from QwtAbstractScaleDraw::label().
*/

void QwtScaleDraw::setLabelAlignment( Qt::Alignment alignment )
{
    d_data->labelAlignment = alignment;
}

/*!
  \return the label flags
  \sa setLabelAlignment(), labelRotation()
*/
Qt::Alignment QwtScaleDraw::labelAlignment() const
{
    return d_data->labelAlignment;
}

/*!
  \param font Font
  \return the maximum width of a label
*/
int QwtScaleDraw::maxLabelWidth( const QFont &font ) const
{
    double maxWidth = 0.0;

    const QList<double> &ticks = scaleDiv().ticks( QwtScaleDiv::MajorTick );
    for ( int i = 0; i < ticks.count(); i++ )
    {
        const double v = ticks[i];
        if ( scaleDiv().contains( v ) )
        {
            const double w = labelSize( font, ticks[i] ).width();
            if ( w > maxWidth )
                maxWidth = w;
        }
    }

    return qCeil( maxWidth );
}

/*!
  \param font Font
  \return the maximum height of a label
*/
int QwtScaleDraw::maxLabelHeight( const QFont &font ) const
{
    double maxHeight = 0.0;

    const QList<double> &ticks = scaleDiv().ticks( QwtScaleDiv::MajorTick );
    for ( int i = 0; i < ticks.count(); i++ )
    {
        const double v = ticks[i];
        if ( scaleDiv().contains( v ) )
        {
            const double h = labelSize( font, ticks[i] ).height();
            if ( h > maxHeight )
                maxHeight = h;
        }
    }

    return qCeil( maxHeight );
}

void QwtScaleDraw::updateMap()
{
    const QPointF pos = d_data->pos;
    double len = d_data->len;

    QwtScaleMap &sm = scaleMap();
    if ( orientation() == Qt::Vertical )
        sm.setPaintInterval( pos.y() + len, pos.y() );
    else
        sm.setPaintInterval( pos.x(), pos.x() + len );
}
