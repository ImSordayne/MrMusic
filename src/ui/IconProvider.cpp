#include "ui/IconProvider.h"

#include "core/Logger.h"

#include <QColor>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QUrlQuery>

namespace mr {

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage IconProvider::render(const QString &name, const QColor &color, int pixels)
{
    QFile file(QStringLiteral(":/res/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::warn(QStringLiteral("icon '%1' not found in resources").arg(name));
        return {};
    }
    QByteArray svg = file.readAll();
    file.close();

    // Every SVG in res/ paints with `currentColor`; Qt's SVG reader has no CSS
    // context, so the colour is substituted before rasterising.
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) {
        Logger::warn(QStringLiteral("icon '%1' is not a valid SVG").arg(name));
        return {};
    }

    const int size = qBound(8, pixels, 512);
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter, QRectF(0, 0, size, size));
    painter.end();
    return image;
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // id == "<name>?c=%23RRGGBB&s=40"
    QString name = id;
    QColor color(Qt::black);
    int pixels = 40;

    const int queryStart = id.indexOf(QLatin1Char('?'));
    if (queryStart >= 0) {
        name = id.left(queryStart);
        QUrlQuery query(id.mid(queryStart + 1));
        if (query.hasQueryItem(QStringLiteral("c"))) {
            const QColor parsed(query.queryItemValue(QStringLiteral("c")));
            if (parsed.isValid())
                color = parsed;
        }
        if (query.hasQueryItem(QStringLiteral("s")))
            pixels = qMax(8, query.queryItemValue(QStringLiteral("s")).toInt());
    }

    if (requestedSize.isValid() && requestedSize.width() > 0)
        pixels = requestedSize.width();

    QImage image = render(name, color, pixels);
    if (size)
        *size = image.size();
    return image;
}

} // namespace mr
