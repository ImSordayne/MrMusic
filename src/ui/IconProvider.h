#pragma once

#include <QQuickImageProvider>
#include <QSize>
#include <QString>

namespace mr {

/// Renders the SVG icons in `res/` with an arbitrary colour.
///
/// URL form: `image://mricon/<name>?c=%23RRGGBB&s=<pixels>`
///
/// The colour replacement happens on the SVG markup (`currentColor`), so the
/// result never depends on shader effects - unlike the previous
/// `MultiEffect`-based tinting, which silently produced black icons on some
/// configurations (TARGET.md §9.12).
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QImage render(const QString &name, const QColor &color, int pixels);
};

} // namespace mr
