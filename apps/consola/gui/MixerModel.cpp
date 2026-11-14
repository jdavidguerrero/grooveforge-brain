#include "MixerModel.h"
#include <cmath>
#include <QDebug>

MixerModel::MixerModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Initialize with 8 default tracks (will be updated from Live)
    m_tracks.reserve(16);  // Reserve for typical project size
    for (int i = 0; i < 8; ++i) {
        MixerTrack track;
        track.index = i;
        track.name = QString("Track %1").arg(i + 1);
        track.tag = QString("T%1").arg(i + 1);
        track.active = true;  // All tracks active by default (updated from Live)
        m_tracks.append(track);
    }
}

int MixerModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_tracks.size();
}

QVariant MixerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
        return {};

    const MixerTrack &track = m_tracks.at(index.row());
    
    switch (role) {
    case IndexRole:      return track.index;
    case NameRole:       return track.name;
    case TagRole:        return track.tag;
    case ColorRole:      return track.color;
    case VolumeRole:     return track.volume;
    case VolumeLabelRole: return track.volumeLabel;
    case PanRole:        return track.pan;
    case PanLabelRole:   return track.panLabel;
    case SendARole:      return track.sendA;
    case SendBRole:      return track.sendB;
    case SendCRole:      return track.sendC;
    case SendDRole:      return track.sendD;
    case MutedRole:      return track.muted;
    case SoloRole:       return track.solo;
    case ArmedRole:      return track.armed;
    case ActiveRole:     return track.active;
    case MeterLRole:     return track.meterL;
    case MeterRRole:     return track.meterR;
    default:             return {};
    }
}

QHash<int, QByteArray> MixerModel::roleNames() const
{
    return {
        { IndexRole,       "index" },
        { NameRole,        "name" },
        { TagRole,         "tag" },
        { ColorRole,       "color" },
        { VolumeRole,      "volume" },
        { VolumeLabelRole, "volumeLabel" },
        { PanRole,         "pan" },
        { PanLabelRole,    "panLabel" },
        { SendARole,       "sendA" },
        { SendBRole,       "sendB" },
        { SendCRole,       "sendC" },
        { SendDRole,       "sendD" },
        { MutedRole,       "muted" },
        { SoloRole,        "solo" },
        { ArmedRole,       "armed" },
        { ActiveRole,      "active" },
        { MeterLRole,      "meterL" },
        { MeterRRole,      "meterR" }
    };
}

void MixerModel::setTrackBank(int bank)
{
    if (m_trackBank == bank)
        return;
    
    m_trackBank = qMax(0, bank);
    emit trackBankChanged();
}

void MixerModel::setSelectedTrackIndex(int index)
{
    if (m_selectedTrackIndex == index)
        return;
    
    m_selectedTrackIndex = qBound(0, index, m_tracks.size() - 1);
    emit selectedTrackIndexChanged();
}

void MixerModel::setShowMasterReturns(bool show)
{
    if (m_showMasterReturns == show)
        return;
    
    m_showMasterReturns = show;
    emit showMasterReturnsChanged();
}

void MixerModel::setTrackName(int trackIndex, const QString &name)
{
    updateTrack(trackIndex, [&](MixerTrack &t) {
        t.name = name;
        // Generate tag from name (first 3-4 chars, uppercase)
        t.tag = name.left(4).toUpper();
    });
}

void MixerModel::setTrackColor(int trackIndex, const QColor &color)
{
    updateTrack(trackIndex, [&](MixerTrack &t) {
        t.color = color;
    });
}

void MixerModel::setTrackVolume(int trackIndex, float volume)
{
    qWarning() << "🎵🎵🎵 MixerModel::setTrackVolume CALLED: track=" << trackIndex << "volume=" << volume;
    qWarning() << "   m_tracks.size()=" << m_tracks.size();

    updateTrack(trackIndex, [&](MixerTrack &t) {
        qWarning() << "   Lambda executing: old volume=" << t.volume << "new volume=" << volume;
        t.volume = qBound(0.0f, volume, 1.0f);
        t.volumeLabel = formatVolumeLabel(t.volume);
        qWarning() << "   Updated: volume=" << t.volume << "label=" << t.volumeLabel;
    });

    qWarning() << "   setTrackVolume completed";
}

void MixerModel::setTrackPan(int trackIndex, float pan)
{
    updateTrack(trackIndex, [&](MixerTrack &t) {
        t.pan = qBound(0.0f, pan, 1.0f);
        t.panLabel = formatPanLabel(t.pan);
    });
}

void MixerModel::setTrackSend(int trackIndex, int sendIndex, float value)
{
    updateTrack(trackIndex, [&](MixerTrack &t) {
        value = qBound(0.0f, value, 1.0f);
        switch (sendIndex) {
        case 0: t.sendA = value; break;
        case 1: t.sendB = value; break;
        case 2: t.sendC = value; break;
        case 3: t.sendD = value; break;
        }
    });
}

void MixerModel::setTrackMuted(int trackIndex, bool muted)
{
    updateTrack(trackIndex, [&](MixerTrack &t) {
        t.muted = muted;
    });
}

void MixerModel::setTrackSolo(int trackIndex, bool solo)
{
    updateTrack(trackIndex, [&](MixerTrack &t) {
        t.solo = solo;
    });
}

void MixerModel::setTrackArmed(int trackIndex, bool armed)
{
    updateTrack(trackIndex, [&](MixerTrack &t) {
        t.armed = armed;
    });
}

void MixerModel::setTrackActive(int trackIndex, bool active)
{
    updateTrack(trackIndex, [&](MixerTrack &t) {
        t.active = active;
    });
}

void MixerModel::setTrackMeter(int trackIndex, float meterL, float meterR)
{
    updateTrack(trackIndex, [&](MixerTrack &t) {
        t.meterL = qBound(0.0f, meterL, 1.0f);
        t.meterR = qBound(0.0f, meterR, 1.0f);
    });
}

void MixerModel::resetAllTracks()
{
    beginResetModel();
    for (auto &track : m_tracks) {
        track.volume = 0.85f;
        track.pan = 0.5f;
        track.sendA = 0.0f;
        track.sendB = 0.0f;
        track.sendC = 0.0f;
        track.sendD = 0.0f;
        track.muted = false;
        track.solo = false;
        track.armed = false;
        track.meterL = 0.0f;
        track.meterR = 0.0f;
        track.volumeLabel = formatVolumeLabel(track.volume);
        track.panLabel = formatPanLabel(track.pan);
    }
    endResetModel();
}

void MixerModel::setTotalTracks(int count)
{
    if (count == m_tracks.size())
        return;

    if (count > m_tracks.size()) {
        // Add new tracks
        beginInsertRows(QModelIndex(), m_tracks.size(), count - 1);
        for (int i = m_tracks.size(); i < count; ++i) {
            MixerTrack track;
            track.index = i;
            track.name = QString("Track %1").arg(i + 1);
            track.tag = QString("T%1").arg(i + 1);
            track.active = true;
            m_tracks.append(track);
        }
        endInsertRows();
    } else {
        // Remove tracks
        beginRemoveRows(QModelIndex(), count, m_tracks.size() - 1);
        m_tracks.resize(count);
        endRemoveRows();
    }
    
    emit totalTracksChanged();
}

int MixerModel::displayedTrackIndex(int localIndex) const
{
    return m_trackBank * tracksPerBank() + localIndex;
}

bool MixerModel::isValidTrack(int trackIndex) const
{
    return trackIndex >= 0 && trackIndex < m_tracks.size();
}

int MixerModel::trackIndexFor(int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return -1;
    return trackIndex;
}

void MixerModel::updateTrack(int trackIndex, std::function<void(MixerTrack&)> updater)
{
    qWarning() << "📊📊📊 MixerModel::updateTrack: trackIndex=" << trackIndex << "m_tracks.size()=" << m_tracks.size();

    int idx = trackIndexFor(trackIndex);
    qWarning() << "   trackIndexFor returned:" << idx;

    if (idx < 0) {
        qWarning() << "   ❌ Invalid index, returning early";
        return;
    }

    qWarning() << "   Calling updater lambda...";
    updater(m_tracks[idx]);

    const QModelIndex modelIndex = this->index(idx, 0);
    qWarning() << "   Emitting dataChanged for row:" << idx;
    emit dataChanged(modelIndex, modelIndex);
    qWarning() << "   ✅ updateTrack completed";
}

QString MixerModel::formatVolumeLabel(float volume) const
{
    if (volume < 0.001f)
        return "-∞";
    
    // Convert linear 0-1 to dB (approximation)
    // 0.85 ≈ -1.5 dB, 1.0 = 0 dB
    float db = 20.0f * std::log10(volume);
    
    if (db > -0.5f)
        return QString("0.0 dB");
    else if (db < -60.0f)
        return "-∞";
    else
        return QString("%1 dB").arg(db, 0, 'f', 1);
}

QString MixerModel::formatPanLabel(float pan) const
{
    if (pan < 0.48f || pan > 0.52f) {
        // Not center
        int steps = static_cast<int>((pan - 0.5f) * 50.0f);  // -25 to +25
        if (steps < 0)
            return QString("L%1").arg(-steps);
        else
            return QString("R%1").arg(steps);
    }
    return "C";  // Center
}
