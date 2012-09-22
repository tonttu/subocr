#ifndef LINE_SPLITTER_HPP
#define LINE_SPLITTER_HPP

#include <QList>
#include <QImage>

namespace LineSplitter
{
  QList<QImage> split(const QImage& img);
  QList<QImage> splitPath(const QImage& img);
}

#endif // LINE_SPLITTER_HPP
