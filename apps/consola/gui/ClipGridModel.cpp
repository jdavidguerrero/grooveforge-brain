#include "ClipGridModel.h"

ClipGridModel::ClipGridModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_clips.reserve(32);
    for (int scene = 0; scene < 4; ++scene) {
        for (int track = 0; track < 8; ++track) {
            ClipCell cell;
            cell.track = track;
            cell.scene = scene;
            m_clips.append(cell);
        }
    }
}

int ClipGridModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_clips.size();
}

QVariant ClipGridModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_clips.size())
        return {};

    const ClipCell &clip = m_clips.at(index.row());
    switch (role) {
    case TrackRole:
        return clip.track;
    case SceneRole:
        return clip.scene;
    case NameRole:
        return clip.name;
    case StateRole:
        return clip.state;
    case ColorRole:
        return clip.color;
    default:
        return {};
    }
}

QHash<int, QByteArray> ClipGridModel::roleNames() const
{
    return {
        { TrackRole, "track" },
        { SceneRole, "scene" },
        { NameRole, "name" },
        { StateRole, "state" },
        { ColorRole, "color" }
    };
}

void ClipGridModel::setClipName(int track, int scene, const QString &name)
{
    int idx = indexFor(track, scene);
    if (idx < 0)
        return;
    if (m_clips[idx].name == name)
        return;
    m_clips[idx].name = name;
    const QModelIndex modelIndex = this->index(idx, 0);
    emit dataChanged(modelIndex, modelIndex, { NameRole });
}

void ClipGridModel::setClipColor(int track, int scene, const QColor &color)
{
    int idx = indexFor(track, scene);
    if (idx < 0)
        return;
    if (m_clips[idx].color == color)
        return;
    m_clips[idx].color = color;
    const QModelIndex modelIndex = this->index(idx, 0);
    emit dataChanged(modelIndex, modelIndex, { ColorRole });
}

void ClipGridModel::setClipState(int track, int scene, int state)
{
    int idx = indexFor(track, scene);
    if (idx < 0)
        return;
    if (m_clips[idx].state == state)
        return;
    m_clips[idx].state = state;
    const QModelIndex modelIndex = this->index(idx, 0);
    emit dataChanged(modelIndex, modelIndex, { StateRole });
}

void ClipGridModel::resetAll(const QColor &color)
{
    for (ClipCell &clip : m_clips) {
        clip.color = color;
        clip.name.clear();
        clip.state = 0;
    }
    if (!m_clips.isEmpty()) {
        const QModelIndex first = this->index(0, 0);
        const QModelIndex last = this->index(m_clips.size() - 1, 0);
        emit dataChanged(first, last);
    }
}

int ClipGridModel::indexFor(int track, int scene) const
{
    if (track < 0 || track >= 8 || scene < 0 || scene >= 4)
        return -1;
    return scene * 8 + track;
}
