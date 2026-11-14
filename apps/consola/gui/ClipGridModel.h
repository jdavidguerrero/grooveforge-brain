#ifndef CLIPGRIDMODEL_H
#define CLIPGRIDMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QVector>
#include <QString>

struct ClipCell {
    int track = 0;
    int scene = 0;
    QString name;
    int state = 0;
    QColor color = QColor("#282828");
};

class ClipGridModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        TrackRole = Qt::UserRole + 1,
        SceneRole,
        NameRole,
        StateRole,
        ColorRole
    };
    Q_ENUM(Roles)

    explicit ClipGridModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setClipName(int track, int scene, const QString &name);
    void setClipColor(int track, int scene, const QColor &color);
    void setClipState(int track, int scene, int state);
    void resetAll(const QColor &color);

private:
    int indexFor(int track, int scene) const;

    QVector<ClipCell> m_clips;
};

#endif // CLIPGRIDMODEL_H
