#include "account/AccountStore.h"
#include "api/KugouClient.h"
#include "api/NeteaseClient.h"
#include "core/AppPaths.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "core/SingleInstance.h"
#include "download/DownloadManager.h"
#include "player/PlayerController.h"
#include "ui/AccountController.h"
#include "ui/AppController.h"
#include "ui/BrowseController.h"
#include "ui/TrackListModel.h"
#include "ui/IconProvider.h"
#include "ui/LibraryController.h"

#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQmlContext>
#include <QTranslator>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QQuickWindow>
#include <QSvgRenderer>
#include <QTimer>
#include <QCommandLineParser>

using namespace mr;

int main(int argc, char *argv[])
{
    // Qt Multimedia is asked to use its FFmpeg backend on every platform, which
    // keeps the audio pipeline identical (TARGET.md §2.3).
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");

    // MrMusic draws its own controls, so the fully customisable "Basic" style is
    // used instead of the native Windows style (which forbids customisation).
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("MrMusic"));
    QGuiApplication::setOrganizationName(QStringLiteral("MrMusic"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("mrmusic.local"));
    QGuiApplication::setApplicationVersion(QStringLiteral(MRMUSIC_VERSION));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("MrMusic"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/res/app.png")));

    // --- icon rendering helper --------------------------------------------
    // `--render-svg <in.svg> <out.png> <px> [background]` rasterises an SVG with
    // QSvgRenderer. Used by scripts/render-icons.ps1 to keep the window, tray,
    // installer and Android launcher icons in sync with UiDesign/Logo.svg.
    {
        const QStringList renderArgs = QCoreApplication::arguments();
        const int index = renderArgs.indexOf(QStringLiteral("--render-svg"));
        if (index >= 0 && index + 3 < renderArgs.size()) {
            const QString source = renderArgs.at(index + 1);
            const QString target = renderArgs.at(index + 2);
            const int pixels = qMax(8, renderArgs.at(index + 3).toInt());
            QString background;
            if (index + 4 < renderArgs.size() && !renderArgs.at(index + 4).startsWith(QLatin1String("--")))
                background = renderArgs.at(index + 4);

            // "#RRGGBB" = flat plate, "#RRGGBB:round" = rounded tile (app icon)
            bool rounded = false;
            if (background.endsWith(QLatin1String(":round"))) {
                rounded = true;
                background.chop(6);
            }

            QSvgRenderer renderer(source);
            if (!renderer.isValid()) {
                fprintf(stderr, "cannot read %s\n", qPrintable(source));
                return 1;
            }
            QImage image(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            {
                QPainter plate(&image);
                plate.setRenderHint(QPainter::Antialiasing, true);
                if (!background.isEmpty()) {
                    plate.setPen(Qt::NoPen);
                    plate.setBrush(QColor(background));
                    if (rounded)
                        plate.drawRoundedRect(QRectF(0, 0, pixels, pixels), pixels * 0.22, pixels * 0.22);
                    else
                        plate.drawRect(QRectF(0, 0, pixels, pixels));
                }
            }
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            const qreal inset = background.isEmpty() ? 0.0 : pixels * 0.14;
            renderer.render(&painter, QRectF(inset, inset, pixels - 2 * inset, pixels - 2 * inset));
            painter.end();
            if (!image.save(target)) {
                fprintf(stderr, "cannot write %s\n", qPrintable(target));
                return 1;
            }
            return 0;
        }
    }

    AppPaths::initialize();
    Logger::initialize();
    Logger::setDebugEnabled(qEnvironmentVariableIsSet("MRMUSIC_DEBUG"));

    Logger::info(QStringLiteral("MrMusic %1 starting, data dir %2")
                     .arg(QStringLiteral(MRMUSIC_VERSION), AppPaths::dataDir()));

    Settings settings;
    settings.load();

    QTranslator translator;
    const QString locale = settings.locale();
    if (locale.startsWith(QLatin1String("en"))
        && translator.load(QStringLiteral(":/res/i18n/mrmusic_en_US.qm"))) {
        QCoreApplication::installTranslator(&translator);
    }

    // --- single instance (TARGET.md §9.8) ---------------------------------
    // The developer switches imply automation, so they bypass the guard.
    const QStringList startupArguments = QCoreApplication::arguments();
    const bool automation = startupArguments.contains(QStringLiteral("--screenshot"))
        || startupArguments.contains(QStringLiteral("--allow-multiple"));

    SingleInstance instance(QStringLiteral("MrMusic-single-instance"), &app);
    if (!automation && !instance.isPrimary()) {
        if (instance.notifyPrimary())
            Logger::info(QStringLiteral("another instance is running, asked it to show its window"));
        else
            Logger::warn(QStringLiteral("another instance is running but could not be reached"));
        return 0;
    }

    NeteaseClient netease;
    KugouClient kugou;
    if (settings.proxyEnabled()) {
        netease.setProxy(settings.proxyUrl());
        kugou.setProxy(settings.proxyUrl());
    }

    AccountStore accountStore;
    AccountController accounts(&netease, &kugou, &accountStore);
    accounts.restore();

    LibraryController library(&netease, &kugou);
    library.load();

    AppController appController(&netease, &kugou, &settings);
    BrowseController browse(&netease, &kugou);
    PlayerController player(&netease, &kugou, &settings);
    DownloadManager downloads(&netease, &kugou, &settings);

    appController.setFavoriteKeys(library.favoriteKeyList());
    player.setFavoriteKeys(library.favoriteKeyList());

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("mricon"), new IconProvider);
    QQmlContext *context = engine.rootContext();
    context->setContextProperty(QStringLiteral("app"), &appController);
    context->setContextProperty(QStringLiteral("browse"), &browse);
    context->setContextProperty(QStringLiteral("player"), &player);
    context->setContextProperty(QStringLiteral("library"), &library);
    context->setContextProperty(QStringLiteral("accounts"), &accounts);
    context->setContextProperty(QStringLiteral("downloads"), &downloads);
    context->setContextProperty(QStringLiteral("settings"), &settings);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.loadFromModule("MrMusic", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    if (QObject *root = engine.rootObjects().constFirst()) {
        QObject::connect(&instance, &SingleInstance::activationRequested, root, [root]() {
            QMetaObject::invokeMethod(root, "restoreWindow");
        });
    }

    // --- developer helpers (used by the CI smoke test and for screenshots) ---
    const QStringList arguments = startupArguments;
    const int kindIndex = arguments.indexOf(QStringLiteral("--search-kind"));
    if (kindIndex >= 0 && kindIndex + 1 < arguments.size())
        appController.setSearchKind(arguments.at(kindIndex + 1).toInt());

    const int searchIndex = arguments.indexOf(QStringLiteral("--search"));
    if (searchIndex >= 0 && searchIndex + 1 < arguments.size())
        appController.search(arguments.at(searchIndex + 1));

    const int sizeIndex = arguments.indexOf(QStringLiteral("--size"));
    if (sizeIndex >= 0 && sizeIndex + 1 < arguments.size()) {
        const QString spec = arguments.at(sizeIndex + 1);
        const QStringList parts = spec.split(QLatin1Char('x'));
        if (parts.size() == 2) {
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
                window->resize(parts.at(0).toInt(), parts.at(1).toInt());
            }
        }
    }

    const int downloadDirIndex = arguments.indexOf(QStringLiteral("--download-dir"));
    if (downloadDirIndex >= 0 && downloadDirIndex + 1 < arguments.size())
        settings.setDownloadDir(arguments.at(downloadDirIndex + 1));

    if (arguments.contains(QStringLiteral("--download-first"))) {
        QTimer::singleShot(5000, &downloads, [&appController, &downloads]() {
            const QVariantList tracks = appController.searchResults()->toVariantList();
            if (!tracks.isEmpty())
                downloads.enqueue(tracks.first().toMap());
        });
    }

    if (arguments.contains(QStringLiteral("--play-first"))) {
        QTimer::singleShot(5000, &player, [&appController, &player]() {
            const QVariantList tracks = appController.searchResults()->toVariantList();
            if (!tracks.isEmpty())
                player.playTracks(tracks, 0);
        });
    }


    const int quitIndex = arguments.indexOf(QStringLiteral("--quit-after"));
    if (quitIndex >= 0 && quitIndex + 1 < arguments.size()) {
        const int delay = arguments.at(quitIndex + 1).toInt();
        // exactly the same slot the tray menu calls
        QTimer::singleShot(delay, &appController, [&appController]() { appController.quitApplication(); });
    }

    const int shotIndex = arguments.indexOf(QStringLiteral("--screenshot"));
    if (shotIndex >= 0 && shotIndex + 1 < arguments.size()) {
        const QString target = arguments.at(shotIndex + 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        int delayMs = 3500;
        const int delayIndex = arguments.indexOf(QStringLiteral("--screenshot-delay"));
        if (delayIndex >= 0 && delayIndex + 1 < arguments.size())
            delayMs = arguments.at(delayIndex + 1).toInt();
        QTimer::singleShot(delayMs, window, [window, target]() {
            if (!window)
                return;
            const QImage image = window->grabWindow();
            Logger::info(QStringLiteral("screenshot %1 -> %2").arg(image.size().width()).arg(target));
            image.save(target);
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
