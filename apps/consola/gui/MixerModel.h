#ifndef MIXERMODEL_H
#define MIXERMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QVector>
#include <QString>

// ═══════════════════════════════════════════════════════════
// MIXER TRACK STRUCTURE
// ═══════════════════════════════════════════════════════════
struct MixerTrack {
    int index = 0;
    QString name = "---";
    QString tag = "---";
    QColor color = QColor("#808080");
    
    // Mixer parameters
    float volume = 0.85f;      // 0.0 - 1.0 (0.85 = -1.5 dB typical)
    float pan = 0.5f;          // 0.0 - 1.0 (0.5 = center)
    float sendA = 0.0f;        // 0.0 - 1.0
    float sendB = 0.0f;        // 0.0 - 1.0
    float sendC = 0.0f;        // 0.0 - 1.0 (future)
    float sendD = 0.0f;        // 0.0 - 1.0 (future)
    
    // States
    bool muted = false;
    bool solo = false;
    bool armed = false;
    bool active = true;        // Track exists in Live
    
    // Metering (VU meters)
    float meterL = 0.0f;       // 0.0 - 1.0
    float meterR = 0.0f;       // 0.0 - 1.0
    
    // Display strings
    QString volumeLabel = "-1.5 dB";
    QString panLabel = "C";
};

// ═══════════════════════════════════════════════════════════
// MIXER MODEL - Manages all mixer tracks
// ═══════════════════════════════════════════════════════════
class MixerModel : public QAbstractListModel
{
    Q_OBJECT
    
    // Properties exposed to QML
    Q_PROPERTY(int trackBank READ trackBank WRITE setTrackBank NOTIFY trackBankChanged)
    Q_PROPERTY(int tracksPerBank READ tracksPerBank CONSTANT)
    Q_PROPERTY(int totalTracks READ totalTracks NOTIFY totalTracksChanged)
    Q_PROPERTY(int selectedTrackIndex READ selectedTrackIndex WRITE setSelectedTrackIndex NOTIFY selectedTrackIndexChanged)
    Q_PROPERTY(bool showMasterReturns READ showMasterReturns WRITE setShowMasterReturns NOTIFY showMasterReturnsChanged)
    
public:
    enum Roles {
        IndexRole = Qt::UserRole + 1,
        NameRole,
        TagRole,
        ColorRole,
        VolumeRole,
        VolumeLabelRole,
        PanRole,
        PanLabelRole,
        SendARole,
        SendBRole,
        SendCRole,
        SendDRole,
        MutedRole,
        SoloRole,
        ArmedRole,
        ActiveRole,
        MeterLRole,
        MeterRRole
    };
    Q_ENUM(Roles)

    explicit MixerModel(QObject *parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Property getters
    int trackBank() const { return m_trackBank; }
    int tracksPerBank() const { return 4; }  // Hardware limitation: 4 faders
    int totalTracks() const { return m_tracks.size(); }
    int selectedTrackIndex() const { return m_selectedTrackIndex; }
    bool showMasterReturns() const { return m_showMasterReturns; }

    // Property setters
    void setTrackBank(int bank);
    void setSelectedTrackIndex(int index);
    void setShowMasterReturns(bool show);

    // Track updates (called by SerialController)
    void setTrackName(int trackIndex, const QString &name);
    void setTrackColor(int trackIndex, const QColor &color);
    void setTrackVolume(int trackIndex, float volume);
    void setTrackPan(int trackIndex, float pan);
    void setTrackSend(int trackIndex, int sendIndex, float value);
    void setTrackMuted(int trackIndex, bool muted);
    void setTrackSolo(int trackIndex, bool solo);
    void setTrackArmed(int trackIndex, bool armed);
    void setTrackActive(int trackIndex, bool active);
    void setTrackMeter(int trackIndex, float meterL, float meterR);

    // Bulk updates
    void resetAllTracks();
    void setTotalTracks(int count);

    // Helpers
    Q_INVOKABLE int displayedTrackIndex(int localIndex) const;
    Q_INVOKABLE bool isValidTrack(int trackIndex) const;

signals:
    void trackBankChanged();
    void totalTracksChanged();
    void selectedTrackIndexChanged();
    void showMasterReturnsChanged();
    
    // Signal when user changes values (to send to hardware)
    void trackVolumeChangeRequested(int trackIndex, float volume);
    void trackPanChangeRequested(int trackIndex, float pan);
    void trackSendChangeRequested(int trackIndex, int sendIndex, float value);
    void trackMuteToggleRequested(int trackIndex);
    void trackSoloToggleRequested(int trackIndex);
    void trackArmToggleRequested(int trackIndex);

private:
    QVector<MixerTrack> m_tracks;
    int m_trackBank = 0;
    int m_selectedTrackIndex = 0;
    bool m_showMasterReturns = false;

    int trackIndexFor(int trackIndex) const;
    void updateTrack(int trackIndex, std::function<void(MixerTrack&)> updater);
    QString formatVolumeLabel(float volume) const;
    QString formatPanLabel(float pan) const;
};

#endif // MIXERMODEL_H
