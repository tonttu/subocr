#include "line_splitter.hpp"

#include <QDebug>

namespace LineSplitter
{
  QList<QImage> split(const QImage& img)
  {
    QVector<bool> emptyLines(img.height(), true);
    for (int y = 0; y < img.height(); ++y) {
      const uchar* it = img.constScanLine(y);
      const uchar* end = it + img.width();
      while (it < end) {
        if (*it++ > 10) {
          emptyLines[y] = false;
          break;
        }
      }
    }

    QList<QImage> lines;
    for (int y = 0; y < img.height();) {
      if (emptyLines[y]) {
        ++y;
        continue;
      }

      QRect r(0, y, img.width(), 1);
      int height = 0;
      for (; y < img.height() && !emptyLines[y]; ++y)
        ++height;
      r.setHeight(height);
      // qDebug() << r;
      lines << img.copy(r);
    }
    return lines;
  }
}
