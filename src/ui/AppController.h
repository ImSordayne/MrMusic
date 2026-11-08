#pragma once

#include "api/ApiTypes.h"
#include "ui/CatalogModel.h"
#include "ui/TrackListModel.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace mr {

class KugouClient;
class NeteaseClient;
class Settings;
class TrackListModel;

/// Search (merged across both platforms) and the two daily recommendation
/// entries shown on the home page (TARGET.md §2.1 / §2.2).
class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(mr::TrackListModel *searchResults READ searchResults CONSTANT)
    Q_PROPERTY(mr::CatalogModel *albumResults READ albumResults CONSTANT)
    Q_PROPERTY(mr::CatalogModel *mvResults READ mvResults CONSTANT)
    Q_PROPERTY(int searchKind READ searchKind WRITE setSearchKind NOTIFY searchKindChanged)
    Q_PROPERTY(mr::TrackListModel *neteaseDaily READ neteaseDaily CONSTANT)
    Q_PROPERTY(mr::TrackListModel *kugouDaily READ kugouDaily CONSTANT)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(bool loadingDaily READ loadingDaily NOTIFY loadingDailyChanged)
    Q_PROPERTY(QString searchMessage READ searchMessage NOTIFY searchMessageChanged)
    Q_PROPERTY(QString dailyMessage READ dailyMessage NOTIFY dailyMessageChanged)
    Q_PROPERTY(QString keyword READ keyword NOTIFY keywordChanged)
    Q_PROPERTY(int sourceFilter READ sourceFilter WRITE setSourceFilter NOTIFY sourceFilterChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)

public:
    AppController(NeteaseClient *netease, KugouClient *kugou, Settings *settings, QObject *parent = nullptr);

    TrackListModel *searchResults() const { return m_searchResults; }
    CatalogModel *albumResults() const { return m_albumResults; }
    CatalogModel *mvResults() const { return m_mvResults; }
    int searchKind() const { return m_searchKind; }
    TrackListModel *neteaseDaily() const { return m_neteaseDaily; }
    TrackListModel *kugouDaily() const { return m_kugouDaily; }
    bool searching() const { return m_searching; }
    bool loadingDaily() const { return m_loadingDaily; }
    QString searchMessage() const { return m_searchMessage; }
    QString dailyMessage() const { return m_dailyMessage; }
    QString keyword() const { return m_keyword; }
    int sourceFilter() const { return m_sourceFilter; }
    QString version() const;
    QString dataDir() const;

public slots:
    void search(const QString &keyword);
    void searchMore();
    void refreshDaily();
    void setSourceFilter(int filter);
    void setSearchKind(int kind);
    void setFavoriteKeys(const QStringList &keys);
    /// Terminates the process (used by the tray menu, TARGET.md §9.10).
    void quitApplication();

signals:
    void searchingChanged();
    void loadingDailyChanged();
    void searchMessageChanged();
    void dailyMessageChanged();
    void keywordChanged();
    void sourceFilterChanged();
    void searchKindChanged();

private:
    void applyFilter();
    void rebuildMerged();
    void searchCatalogue();
    void searchMoreCatalogue();

    NeteaseClient *m_netease = nullptr;
    KugouClient *m_kugou = nullptr;
    Settings *m_settings = nullptr;

    TrackListModel *m_searchResults = nullptr;
    CatalogModel *m_albumResults = nullptr;
    CatalogModel *m_mvResults = nullptr;
    int m_searchKind = 0;
    int m_catalogueOffset = 0;
    int m_cataloguePage = 1;
    TrackListModel *m_neteaseDaily = nullptr;
    TrackListModel *m_kugouDaily = nullptr;

    QList<Track> m_neteaseHits;
    QList<Track> m_kugouHits;
    QList<Track> m_merged;

    bool m_searching = false;
    bool m_loadingDaily = false;
    bool m_neteasePending = false;
    bool m_kugouPending = false;
    int m_offset = 0;
    int m_sourceFilter = 0; // 0 = all, 1 = netease, 2 = kugou
    QString m_keyword;
    QString m_searchMessage;
    QString m_dailyMessage;
};

} // namespace mr
