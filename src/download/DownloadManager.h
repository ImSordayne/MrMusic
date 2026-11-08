#pragma once

#include "api/ApiTypes.h"

#include <QList>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace mr {

class HttpClient;
class KugouClient;
class NeteaseClient;
class Settings;

/// Download queue honouring the limits from TARGET.md §2.4:
///  * one connection per file (no ranged/multi part downloads),
///  * at most 5 concurrent downloads **per platform**,
///  * file name `作者-专辑-音质-平台.<ext>`,
///  * tags / lyrics / cover written after the transfer completes.
class DownloadManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY itemsChanged)
    Q_PROPERTY(QString targetDir READ targetDir WRITE setTargetDir NOTIFY targetDirChanged)
    Q_PROPERTY(int maxPerSource READ maxPerSource CONSTANT)

public:
    DownloadManager(NeteaseClient *netease, KugouClient *kugou, Settings *settings,
                    QObject *parent = nullptr);

    QVariantList items() const { return m_items; }
    int activeCount() const { return m_activeCount; }
    QString targetDir() const;
    int maxPerSource() const { return 5; }

public slots:
    void enqueue(const QVariantMap &track);
    void enqueueList(const QVariantList &tracks);
    void retry(int index);
    void remove(int index);
    void clearFinished();
    Q_INVOKABLE void openFolder() const;
    void setTargetDir(const QString &dir);

signals:
    void itemsChanged();
    void targetDirChanged();
    void finished(const QString &filePath);
    void failed(const QString &title, const QString &message);

private:
    struct Item
    {
        Track track;
        QString state;   // queued / resolving / downloading / tagging / done / error
        QString message;
        double progress = 0.0;
        QString filePath;
        QString qualityText;
    };

    void pump();
    void startItem(int index);
    void updateItem(int index, const QString &state, const QString &message, double progress);
    int activeForSource(const QString &source) const;
    static QString sanitize(const QString &text);

    NeteaseClient *m_netease = nullptr;
    KugouClient *m_kugou = nullptr;
    Settings *m_settings = nullptr;
    HttpClient *m_http = nullptr;

    QList<Item> m_queue;
    QVariantList m_items;
    int m_activeCount = 0;

    void rebuildItems();
    void resolveAndDownload(int index);
    void writeTagsAndFinish(int index, const QString &partPath, const QString &fullPath, const Track &track);
};

} // namespace mr
