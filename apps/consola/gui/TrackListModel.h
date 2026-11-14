#ifndef TRACKLISTMODEL_H
#define TRACKLISTMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QVector>
#include <QString>

struct TrackInfo {
    int index = 0;
    QString name;
    QColor color = QColor("#2a2a2a");
    bool active = false;
};

class TrackListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IndexRole = Qt::UserRole + 1,
        NameRole,
        ColorRole,
        ActiveRole
    };
    Q_ENUM(Roles)

    explicit TrackListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setTrackName(int index, const QString &name);
    void setTrackColor(int index, const QColor &color);
    void resetAll();
    void clearAbove(int lastActiveIndex);

private:
    bool validIndex(int index) const;
    QVector<TrackInfo> m_tracks;
};

#endif // TRACKLISTMODEL_H
