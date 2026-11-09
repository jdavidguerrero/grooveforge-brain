#include "TrackListModel.h"
#include <QtGlobal>

namespace {
const QColor kEmptyTrackColor(QStringLiteral("#2a2a2a"));
}

TrackListModel::TrackListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_tracks.reserve(8);
    for (int i = 0; i < 8; ++i) {
        TrackInfo track;
        track.index = i;
        track.color = kEmptyTrackColor;
        track.active = false;
        m_tracks.append(track);
    }
}

int TrackListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_tracks.size();
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
        return {};

    const TrackInfo &track = m_tracks.at(index.row());
    switch (role) {
    case IndexRole:
        return track.index;
    case NameRole:
        return track.name;
    case ColorRole:
        return track.color;
    case ActiveRole:
        return track.active;
    default:
        return {};
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    return {
        { IndexRole, "index" },
        { NameRole, "name" },
        { ColorRole, "color" },
        { ActiveRole, "active" }
    };
}

void TrackListModel::setTrackName(int index, const QString &name)
{
    if (!validIndex(index))
        return;
    TrackInfo &track = m_tracks[index];
    const bool hasData = !name.isEmpty();
    QVector<int> roles;

    if (track.name != name) {
        track.name = name;
        roles.append(NameRole);
    }

    if (!hasData && track.color != kEmptyTrackColor) {
        track.color = kEmptyTrackColor;
        if (!roles.contains(ColorRole))
            roles.append(ColorRole);
    }

    if (track.active != hasData) {
        track.active = hasData;
        roles.append(ActiveRole);
    }

    if (roles.isEmpty())
        return;

    const QModelIndex modelIndex = this->index(index, 0);
    emit dataChanged(modelIndex, modelIndex, roles);
}

void TrackListModel::setTrackColor(int index, const QColor &color)
{
    if (!validIndex(index))
        return;
    TrackInfo &track = m_tracks[index];
    if (track.color == color && track.active)
        return;
    track.color = color;
    if (color != kEmptyTrackColor && !track.active)
        track.active = true;
    const QModelIndex modelIndex = this->index(index, 0);
    emit dataChanged(modelIndex, modelIndex, { ColorRole, ActiveRole });
}

void TrackListModel::resetAll()
{
    if (m_tracks.isEmpty())
        return;

    for (TrackInfo &track : m_tracks) {
        track.name.clear();
        track.color = kEmptyTrackColor;
        track.active = false;
    }

    const QModelIndex first = this->index(0, 0);
    const QModelIndex last = this->index(m_tracks.size() - 1, 0);
    emit dataChanged(first, last);
}

void TrackListModel::clearAbove(int lastActiveIndex)
{
    if (m_tracks.isEmpty())
        return;

    int start = qMax(0, lastActiveIndex + 1);
    if (start >= m_tracks.size())
        return;

    bool changed = false;
    for (int i = start; i < m_tracks.size(); ++i) {
        TrackInfo &track = m_tracks[i];
        if (!track.active && track.name.isEmpty() && track.color == kEmptyTrackColor)
            continue;
        track.name.clear();
        track.color = kEmptyTrackColor;
        track.active = false;
        changed = true;
    }

    if (changed) {
        const QModelIndex first = this->index(start, 0);
        const QModelIndex last = this->index(m_tracks.size() - 1, 0);
        emit dataChanged(first, last);
    }
}

bool TrackListModel::validIndex(int index) const
{
    return index >= 0 && index < m_tracks.size();
}
