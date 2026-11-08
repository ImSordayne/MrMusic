#include "ui/CatalogModel.h"

#include <QJsonObject>

namespace mr {

CatalogModel::CatalogModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CatalogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant CatalogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const QVariantMap &row = m_rows.at(index.row());
    switch (role) {
    case NameRole:
        return row.value(QStringLiteral("name"));
    case SubtitleRole:
        return row.value(QStringLiteral("subtitle"));
    case CoverRole:
        return row.value(QStringLiteral("coverUrl"));
    case KeyRole:
        return row.value(QStringLiteral("key"));
    case IdRole:
        return row.value(QStringLiteral("id"));
    case CatalogIdRole:
        return row.value(QStringLiteral("id"));
    case SourceRole:
        return row.value(QStringLiteral("source"));
    case SourceNameRole:
        return row.value(QStringLiteral("sourceName"));
    case ArtistRole:
        return row.value(QStringLiteral("artist"));
    case ArtistIdRole:
        return row.value(QStringLiteral("artistId"));
    case SubscribedRole:
        return row.value(QStringLiteral("subscribed"));
    case DurationRole:
        return row.value(QStringLiteral("durationText"));
    case RowRole:
        return index.row();
    default:
        break;
    }
    return {};
}

QHash<int, QByteArray> CatalogModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {SubtitleRole, "subtitle"},
        {CoverRole, "coverUrl"},
        {KeyRole, "key"},
        {IdRole, "id"},
        {CatalogIdRole, "catalogId"},
        {SourceRole, "source"},
        {SourceNameRole, "sourceName"},
        {ArtistRole, "artist"},
        {ArtistIdRole, "artistId"},
        {SubscribedRole, "subscribed"},
        {DurationRole, "durationText"},
        {RowRole, "row"},
    };
}

void CatalogModel::setItems(const QVector<QVariantMap> &items)
{
    beginResetModel();
    m_rows = items;
    endResetModel();
    emit countChanged();
}

void CatalogModel::setItems(const QJsonArray &items)
{
    QVector<QVariantMap> rows;
    rows.reserve(items.size());
    for (const QJsonValue &value : items)
        rows.append(value.toObject().toVariantMap());
    setItems(rows);
}

void CatalogModel::append(const QJsonArray &items)
{
    if (items.isEmpty())
        return;
    const int first = m_rows.size();
    beginInsertRows(QModelIndex(), first, first + items.size() - 1);
    for (const QJsonValue &value : items)
        m_rows.append(value.toObject().toVariantMap());
    endInsertRows();
    emit countChanged();
}

void CatalogModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

QVariantList CatalogModel::toVariantList() const
{
    QVariantList list;
    list.reserve(m_rows.size());
    for (const QVariantMap &row : m_rows)
        list.append(row);
    return list;
}

QVariantMap CatalogModel::get(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    return m_rows.at(row);
}

} // namespace mr
