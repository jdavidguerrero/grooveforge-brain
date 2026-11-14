#ifndef SCENELISTMODEL_H
#define SCENELISTMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QVector>
#include <QString>

struct SceneInfo {
    int index = 0;
    QString name;
    QColor color = QColor("#1a1a1a");
    bool triggered = false;
};

class SceneListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IndexRole = Qt::UserRole + 1,
        NameRole,
        ColorRole,
        TriggeredRole
    };
    Q_ENUM(Roles)

    explicit SceneListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setSceneName(int index, const QString &name);
    void setSceneColor(int index, const QColor &color);
    void setSceneTriggered(int index, bool triggered);
    void clearAbove(int lastActiveIndex);

private:
    bool validIndex(int index) const;
    QVector<SceneInfo> m_scenes;
};

#endif // SCENELISTMODEL_H
