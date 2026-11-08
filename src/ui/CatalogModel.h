#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace mr {

/// Generic list model for albums and music videos (TARGET.md §10.19 / §10.20).
///
/// Rows are the `QVariantMap`s produced by `Album::toVariantMap()` /
/// `Mv::toVariantMap()`, so the QML delegates can read
/// `model.name`, `model.subtitle`, `model.coverUrl`, `model.source`, …
class CatalogModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        SubtitleRole,
        CoverRole,
        KeyRole,
        IdRole,
        CatalogIdRole,
        SourceRole,
        SourceNameRole,
        ArtistRole,
        ArtistIdRole,
        SubscribedRole,
        DurationRole,
        RowRole,
    };
    Q_ENUM(Roles)

    explicit CatalogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_rows.size(); }

    void setItems(const QJsonArray &items);
    void setItems(const QVector<QVariantMap> &items);
    void append(const QJsonArray &items);
    void clear();

    QVector<QVariantMap> rows() const { return m_rows; }
    QVariantList toVariantList() const;
    Q_INVOKABLE QVariantMap get(int row) const;

signals:
    void countChanged();

private:
    QVector<QVariantMap> m_rows;
};

} // namespace mr
