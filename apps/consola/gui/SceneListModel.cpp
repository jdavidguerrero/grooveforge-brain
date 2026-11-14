#include "SceneListModel.h"

SceneListModel::SceneListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // default 4 scenes
    m_scenes.reserve(4);
    for (int i = 0; i < 4; ++i) {
        SceneInfo scene;
        scene.index = i;
        scene.name = QStringLiteral("Scene %1").arg(i + 1);
        scene.color = QColor("#1a1a1a");
        scene.triggered = false;
        m_scenes.append(scene);
    }
}

int SceneListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_scenes.size();
}

QVariant SceneListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_scenes.size())
        return {};

    const SceneInfo &scene = m_scenes.at(index.row());
    switch (role) {
    case IndexRole:
        return scene.index;
    case NameRole:
        return scene.name;
    case ColorRole:
        return scene.color;
    case TriggeredRole:
        return scene.triggered;
    default:
        return {};
    }
}

QHash<int, QByteArray> SceneListModel::roleNames() const
{
    return {
        { IndexRole, "index" },
        { NameRole, "name" },
        { ColorRole, "color" },
        { TriggeredRole, "triggered" }
    };
}

void SceneListModel::setSceneName(int index, const QString &name)
{
    if (!validIndex(index))
        return;
    SceneInfo &scene = m_scenes[index];
    if (scene.name == name)
        return;
    scene.name = name;
    const QModelIndex modelIndex = this->index(index, 0);
    emit dataChanged(modelIndex, modelIndex, { NameRole });
}

void SceneListModel::setSceneColor(int index, const QColor &color)
{
    if (!validIndex(index))
        return;
    SceneInfo &scene = m_scenes[index];
    if (scene.color == color)
        return;
    scene.color = color;
    const QModelIndex modelIndex = this->index(index, 0);
    emit dataChanged(modelIndex, modelIndex, { ColorRole });
}

void SceneListModel::setSceneTriggered(int index, bool triggered)
{
    if (!validIndex(index))
        return;
    SceneInfo &scene = m_scenes[index];
    if (scene.triggered == triggered)
        return;
    scene.triggered = triggered;
    const QModelIndex modelIndex = this->index(index, 0);
    emit dataChanged(modelIndex, modelIndex, { TriggeredRole });
}

void SceneListModel::clearAbove(int lastActiveIndex)
{
    if (m_scenes.isEmpty())
        return;

    int start = qMax(0, lastActiveIndex + 1);
    if (start >= m_scenes.size())
        return;

    bool changed = false;
    for (int i = start; i < m_scenes.size(); ++i) {
        SceneInfo &scene = m_scenes[i];
        QString defaultName = QStringLiteral("Scene %1").arg(i + 1);
        QColor defaultColor("#1a1a1a");

        if (scene.name == defaultName && scene.color == defaultColor && !scene.triggered)
            continue;

        scene.name = defaultName;
        scene.color = defaultColor;
        scene.triggered = false;
        changed = true;
    }

    if (changed) {
        const QModelIndex first = this->index(start, 0);
        const QModelIndex last = this->index(m_scenes.size() - 1, 0);
        emit dataChanged(first, last);
    }
}

bool SceneListModel::validIndex(int index) const
{
    return index >= 0 && index < m_scenes.size();
}
