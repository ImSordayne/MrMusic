#include "ui/AppController.h"

#include "api/KugouClient.h"
#include "api/NeteaseClient.h"
#include "core/AppPaths.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "ui/CatalogModel.h"
#include "ui/TrackListModel.h"

#include <QCoreApplication>
#include <QTimer>

#include <cstdlib>
#include <QHash>

#include <memory>

namespace mr {
namespace {
constexpr int kPageSize = 30;
}

AppController::AppController(NeteaseClient *netease, KugouClient *kugou, Settings *settings,
                             QObject *parent)
    : QObject(parent)
    , m_netease(netease)
    , m_kugou(kugou)
    , m_settings(settings)
    , m_searchResults(new TrackListModel(this))
    , m_albumResults(new CatalogModel(this))
    , m_mvResults(new CatalogModel(this))
    , m_neteaseDaily(new TrackListModel(this))
    , m_kugouDaily(new TrackListModel(this))
{
    Q_UNUSED(m_settings)
}

QString AppController::version() const
{
    return QStringLiteral(MRMUSIC_VERSION);
}

void AppController::setSourceFilter(int filter)
{
    filter = qBound(0, filter, 2);
    if (m_sourceFilter == filter)
        return;
    m_sourceFilter = filter;
    emit sourceFilterChanged();
    applyFilter();
}

void AppController::quitApplication()
{
    Logger::info(QStringLiteral("quit requested from the UI"));
    QCoreApplication::exit(0);

    // Safety net: if anything keeps the event loop alive, make sure the process
    // really ends (the tray "quit" entry must never be a no-op).
    QTimer::singleShot(1500, qApp, []() {
        Logger::warn(QStringLiteral("forced exit"));
        std::_Exit(0);
    });
}

QString AppController::dataDir() const
{
    return AppPaths::dataDir();
}

void AppController::setFavoriteKeys(const QStringList &keys)
{
    QSet<QString> set;
    for (const QString &key : keys)
        set.insert(key);
    m_searchResults->setFavoriteKeys(set);
    m_neteaseDaily->setFavoriteKeys(set);
    m_kugouDaily->setFavoriteKeys(set);
}

void AppController::setSearchKind(int kind)
{
    const int clamped = qBound(0, kind, 2);
    if (m_searchKind == clamped)
        return;
    m_searchKind = clamped;
    emit searchKindChanged();
}

void AppController::search(const QString &keyword)
{
    const QString trimmed = keyword.trimmed();
    if (trimmed.isEmpty())
        return;

    if (m_searchKind != 0) {
        m_keyword = trimmed;
        emit keywordChanged();
        m_catalogueOffset = 0;
        m_cataloguePage = 1;
        m_albumResults->clear();
        m_mvResults->clear();
        m_searching = true;
        m_searchMessage = tr("正在搜索两个平台…");
        emit searchingChanged();
        emit searchMessageChanged();
        searchCatalogue();
        return;
    }

    m_keyword = trimmed;
    emit keywordChanged();

    m_offset = 0;
    m_neteaseHits.clear();
    m_kugouHits.clear();
    m_merged.clear();
    m_searchResults->clear();
    m_searching = true;
    m_searchMessage = tr("正在搜索两个平台…");
    emit searchingChanged();
    emit searchMessageChanged();

    m_neteasePending = true;
    m_kugouPending = true;
    m_offset = kPageSize;

    auto finishIfDone = [this]() {
        if (m_neteasePending || m_kugouPending)
            return;
        m_searching = false;
        emit searchingChanged();
        rebuildMerged();
        if (m_searchResults->count() == 0)
            m_searchMessage = tr("没有找到结果");
        else if (m_sourceFilter == 0)
            m_searchMessage = tr("网易云 %1 首 · 酷狗 %2 首")
                                  .arg(m_neteaseHits.size())
                                  .arg(m_kugouHits.size());
        else
            m_searchMessage = tr("共 %1 首").arg(m_searchResults->count());
        emit searchMessageChanged();
    };

    m_netease->search(trimmed, kPageSize, 0, [this, finishIfDone](bool ok, const QJsonObject &body,
                                                                  const QString &error) {
        m_neteasePending = false;
        if (ok) {
            for (const QJsonValue &value : body.value(QStringLiteral("tracks")).toArray())
                m_neteaseHits.append(Track::fromJson(value.toObject()));
        } else {
            Logger::warn(QStringLiteral("netease search failed: %1").arg(error));
            m_searchMessage = tr("网易云搜索失败：%1").arg(error);
        }
        finishIfDone();
    });

    m_kugou->search(trimmed, 1, kPageSize, [this, finishIfDone](bool ok, const QJsonObject &body,
                                                                const QString &error) {
        m_kugouPending = false;
        if (ok) {
            for (const QJsonValue &value : body.value(QStringLiteral("tracks")).toArray())
                m_kugouHits.append(Track::fromJson(value.toObject()));
        } else {
            Logger::warn(QStringLiteral("kugou search failed: %1").arg(error));
        }
        finishIfDone();
    });
}

void AppController::searchMore()
{
    if (m_searchKind != 0) {
        if (m_searching)
            return;
        m_searching = true;
        m_neteasePending = true;
        m_kugouPending = true;
        emit searchingChanged();
        m_catalogueOffset += kPageSize;
        m_cataloguePage += 1;
        searchCatalogue();
        return;
    }

    if (m_keyword.isEmpty() || m_searching)
        return;

    m_searching = true;
    m_neteasePending = true;
    m_kugouPending = true;
    emit searchingChanged();

    auto finishIfDone = [this]() {
        if (m_neteasePending || m_kugouPending)
            return;
        m_searching = false;
        emit searchingChanged();
        rebuildMerged();
    };

    m_netease->search(m_keyword, kPageSize, m_offset, [this, finishIfDone](bool ok, const QJsonObject &body,
                                                                           const QString &) {
        m_neteasePending = false;
        if (ok) {
            QList<Track> extra;
            for (const QJsonValue &value : body.value(QStringLiteral("tracks")).toArray())
                extra.append(Track::fromJson(value.toObject()));
            m_neteaseHits.append(extra);
        }
        finishIfDone();
    });

    m_kugou->search(m_keyword, m_offset / kPageSize + 1, kPageSize, [this, finishIfDone](bool ok,
                                                                                          const QJsonObject &body,
                                                                                          const QString &) {
        m_kugouPending = false;
        if (ok) {
            QList<Track> extra;
            for (const QJsonValue &value : body.value(QStringLiteral("tracks")).toArray())
                extra.append(Track::fromJson(value.toObject()));
            m_kugouHits.append(extra);
        }
        finishIfDone();
    });

    m_offset += kPageSize;
}

void AppController::rebuildMerged()
{
    m_merged.clear();
    if (m_sourceFilter != 2)
        m_merged.append(m_neteaseHits);
    if (m_sourceFilter != 1)
        m_merged.append(m_kugouHits);

    if (m_sourceFilter == 0 && !m_neteaseHits.isEmpty() && !m_kugouHits.isEmpty()) {
        // Interleave both platforms so neither dominates the top of the list.
        QList<Track> interleaved;
        const int maxCount = qMax(m_neteaseHits.size(), m_kugouHits.size());
        interleaved.reserve(m_merged.size());
        for (int i = 0; i < maxCount; ++i) {
            if (i < m_neteaseHits.size())
                interleaved.append(m_neteaseHits.at(i));
            if (i < m_kugouHits.size())
                interleaved.append(m_kugouHits.at(i));
        }
        m_merged = interleaved;
    }
    m_searchResults->setTracks(m_merged);
}

void AppController::applyFilter()
{
    rebuildMerged();
}

void AppController::refreshDaily()
{
    m_loadingDaily = true;
    m_dailyMessage.clear();
    emit loadingDailyChanged();
    emit dailyMessageChanged();

    // shared, because both requests finish asynchronously.
    auto pending = std::make_shared<int>(2);
    auto done = [this, pending]() {
        if (--(*pending) > 0)
            return;
        m_loadingDaily = false;
        emit loadingDailyChanged();
        if (m_neteaseDaily->count() == 0 && m_kugouDaily->count() == 0 && m_dailyMessage.isEmpty())
            m_dailyMessage = tr("暂无推荐内容");
        emit dailyMessageChanged();
    };

    m_netease->dailyRecommend([this, done](bool ok, const QJsonObject &body, const QString &error) {
        if (ok) {
            QList<Track> tracks;
            for (const QJsonValue &value : body.value(QStringLiteral("tracks")).toArray())
                tracks.append(Track::fromJson(value.toObject()));
            m_neteaseDaily->setTracks(tracks);
        } else {
            Logger::warn(QStringLiteral("netease daily failed: %1").arg(error));
            m_neteaseDaily->clear();
            m_dailyMessage = tr("网易云每日推荐需要登录");
        }
        done();
    });

    m_kugou->dailyRecommend([this, done](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            Logger::warn(QStringLiteral("kugou daily failed: %1").arg(error));
            m_kugouDaily->clear();
            done();
            return;
        }

        QList<Track> tracks;
        for (const QJsonValue &value : body.value(QStringLiteral("tracks")).toArray())
            tracks.append(Track::fromJson(value.toObject()));

        if (tracks.isEmpty()) {
            m_kugouDaily->setTracks(tracks);
            done();
            return;
        }

        // The daily endpoint has no cover art: resolve it in one batch request
        // (TARGET.md §9.9).
        m_kugou->resolveCovers(tracks, [this, tracks, done](const QHash<QString, QString> &covers) {
            QList<Track> enriched = tracks;
            int filled = 0;
            for (Track &track : enriched) {
                if (!track.coverUrl.isEmpty())
                    continue;
                const QString url = covers.value(trackKey(track));
                if (!url.isEmpty()) {
                    track.coverUrl = url;
                    ++filled;
                }
            }
            m_kugouDaily->setTracks(enriched);
            if (filled > 0)
                Logger::info(QStringLiteral("kugou daily covers filled: %1").arg(filled));
            done();
        });
    });
}

void AppController::searchCatalogue()
{
    const bool albums = m_searchKind == 1;
    auto finishIfDone = [this, albums]() {
        if (m_neteasePending || m_kugouPending)
            return;
        m_searching = false;
        emit searchingChanged();
        const int total = albums ? m_albumResults->count() : m_mvResults->count();
        m_searchMessage = total == 0 ? tr("没有找到结果")
                                     : (albums ? tr("共 %1 张专辑").arg(total)
                                               : tr("共 %1 个 MV").arg(total));
        emit searchMessageChanged();
    };

    m_neteasePending = true;
    m_kugouPending = true;

    if (albums) {
        m_netease->searchAlbums(m_keyword, kPageSize, m_catalogueOffset,
                                [this, finishIfDone](bool ok, const QJsonObject &body, const QString &error) {
                                    m_neteasePending = false;
                                    if (ok)
                                        m_albumResults->append(body.value(QStringLiteral("albums")).toArray());
                                    else
                                        Logger::warn(QStringLiteral("netease album search failed: %1").arg(error));
                                    finishIfDone();
                                });
        m_kugou->searchAlbums(m_keyword, m_cataloguePage, kPageSize,
                              [this, finishIfDone](bool ok, const QJsonObject &body, const QString &error) {
                                  m_kugouPending = false;
                                  if (ok)
                                      m_albumResults->append(body.value(QStringLiteral("albums")).toArray());
                                  else
                                      Logger::warn(QStringLiteral("kugou album search failed: %1").arg(error));
                                  finishIfDone();
                              });
        return;
    }

    m_netease->searchMvs(m_keyword, kPageSize, m_catalogueOffset,
                         [this, finishIfDone](bool ok, const QJsonObject &body, const QString &error) {
                             m_neteasePending = false;
                             if (ok)
                                 m_mvResults->append(body.value(QStringLiteral("mvs")).toArray());
                             else
                                 Logger::warn(QStringLiteral("netease mv search failed: %1").arg(error));
                             finishIfDone();
                         });
    m_kugou->searchMvs(m_keyword, m_cataloguePage, kPageSize,
                       [this, finishIfDone](bool ok, const QJsonObject &body, const QString &error) {
                           m_kugouPending = false;
                           if (ok)
                               m_mvResults->append(body.value(QStringLiteral("mvs")).toArray());
                           else
                               Logger::warn(QStringLiteral("kugou mv search failed: %1").arg(error));
                           finishIfDone();
                       });
}

} // namespace mr
