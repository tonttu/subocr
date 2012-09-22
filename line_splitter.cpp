#include "line_splitter.hpp"

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

      if (height > 65 && height < 75) {
        lines += splitPath(img.copy(r));
      } else {
        lines << img.copy(r);
      }
    }
    return lines;
  }

  QList<QImage> splitPath(const QImage& img)
  {
    QImage up = img;
    QImage down = img;
    for (int x = 0; x < img.width(); ++x) {
      bool found = false;
      for (int i = 1; i < 10; ++i) {
        int y = img.height() / 2 + i / 2 * ((i%2) ? 1 : -1);
        if (img.pixelIndex(x, y) <= 127) {
          for (int z = 0; z < y; ++z)
            down.setPixel(x, z, 0);
          for (int z = y+1; z < up.height(); ++z)
            up.setPixel(x, z, 0);
          found = true;
          break;
        }
      }
      if (!found) {
        return QList<QImage>() << img;
      }
    }

    return QList<QImage>() << split(up) << split(down);
  }
}
