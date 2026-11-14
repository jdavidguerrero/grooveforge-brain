#ifndef SERIALCONTROLLER_H
#define SERIALCONTROLLER_H

#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <QTimer>
#include <QBitArray>

#include "ClipGridModel.h"
#include "TrackListModel.h"
#include "SceneListModel.h"
#include "MixerModel.h"

class SerialController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(ConnectionState connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(ClipGridModel* clipModel READ clipModel CONSTANT)
    Q_PROPERTY(TrackListModel* trackModel READ trackModel CONSTANT)
    Q_PROPERTY(SceneListModel* sceneModel READ sceneModel CONSTANT)
    Q_PROPERTY(MixerModel* mixerModel READ mixerModel CONSTANT)
    Q_PROPERTY(bool transportPlaying READ transportPlaying NOTIFY transportStateChanged)
    Q_PROPERTY(bool transportRecording READ transportRecording NOTIFY transportRecordingChanged)
    Q_PROPERTY(bool transportLoop READ transportLoop NOTIFY transportStateChanged)
    Q_PROPERTY(bool shiftPressed READ shiftPressed NOTIFY shiftPressedChanged)
    Q_PROPERTY(double transportTempo READ transportTempo NOTIFY transportTempoChanged)
    Q_PROPERTY(QString transportPosition READ transportPosition NOTIFY transportPositionChanged)
    Q_PROPERTY(QString portName READ portName WRITE setPortName NOTIFY portNameChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)
    Q_PROPERTY(int mixerMode READ mixerMode NOTIFY mixerModeChanged)
    Q_PROPERTY(int ringTrackOffset READ ringTrackOffset NOTIFY ringPositionChanged)
    Q_PROPERTY(int ringSceneOffset READ ringSceneOffset NOTIFY ringPositionChanged)

public:
    enum ConnectionState {
        Disconnected,
        WaitingHandshake,
        Connected
    };
    Q_ENUM(ConnectionState)

    explicit SerialController(QObject *parent = nullptr);

    bool isConnected() const { return m_connected; }
    ConnectionState connectionState() const { return m_connectionState; }
    QString portName() const { return m_portName; }
    void setPortName(const QString &name);

    int baudRate() const { return m_baudRate; }
    void setBaudRate(int baud);

    Q_INVOKABLE void reconnect();
    Q_INVOKABLE void requestDisconnect();
    Q_INVOKABLE void sendTransportPlay(bool state);
    Q_INVOKABLE void sendTransportRecord(bool state);
    Q_INVOKABLE void sendTransportLoop(bool state);
    Q_INVOKABLE void sendClipTrigger(int track, int scene);
    Q_INVOKABLE void sendMixerBankChange(int bank);
    Q_INVOKABLE void sendTrackSelect(int trackIndex);

    ClipGridModel* clipModel() const { return m_clipModel; }
    TrackListModel* trackModel() const { return m_trackModel; }
    SceneListModel* sceneModel() const { return m_sceneModel; }
    MixerModel* mixerModel() const { return m_mixerModel; }

    bool transportPlaying() const { return m_transportPlaying; }
    bool transportRecording() const { return m_transportRecording; }
    bool transportLoop() const { return m_transportLoop; }
    bool shiftPressed() const { return m_shiftPressed; }
    double transportTempo() const { return m_transportTempo; }
    QString transportPosition() const { return m_transportPosition; }
    int mixerMode() const { return m_mixerMode; }
    int ringTrackOffset() const { return m_ringTrackOffset; }
    int ringSceneOffset() const { return m_ringSceneOffset; }

signals:
    void connectedChanged();
    void connectionStateChanged();
    void portNameChanged();
    void baudRateChanged();
    void connectionError(const QString &message);
    void transportStateChanged();
    void transportRecordingChanged();
    void shiftPressedChanged();
    void transportTempoChanged();
    void transportPositionChanged();
    void mixerModeChanged(int mode);
    void ringPositionChanged();

private slots:
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);
    void handleReconnectTimeout();
    void handleTrackBatchTimeout();

private:
    void openPort();
    void closePort();
    void processFrame(quint8 cmd, const QByteArray &payload);
    void sendFrame(quint8 cmd, const QByteArray &payload = QByteArray());
    quint8 calculateChecksum(quint8 cmd, quint8 len, const QByteArray &payload) const;
    void setConnected(bool value);
    void setConnectionState(ConnectionState state);
    void handleClipName(const QByteArray &payload);
    void handleGridUpdate7bit(const QByteArray &payload);
    void handleGridUpdate14bit(const QByteArray &payload);
    void handlePadUpdate14bit(const QByteArray &payload);
    void handlePadUpdate7bit(const QByteArray &payload);
    void handleClipState(const QByteArray &payload);
    void updatePadColor(int track, int scene, const QColor &color);
    void handleTrackName(const QByteArray &payload);
    void handleTrackColor(const QByteArray &payload);
    void handleSceneName(const QByteArray &payload);
    void handleSceneColor(const QByteArray &payload);
    void handleSceneTriggered(const QByteArray &payload);
    void handleTransportCommand(quint8 cmd, const QByteArray &payload);
    void scheduleTrackCleanup(int trackIndex);
    
    // Mixer handlers
    void handleMixerVolume(const QByteArray &payload);
    void handleMixerPan(const QByteArray &payload);
    void handleMixerMute(const QByteArray &payload);
    void handleMixerSolo(const QByteArray &payload);
    void handleMixerArm(const QByteArray &payload);
    void handleMixerSend(const QByteArray &payload);
    void handleMixerMode(const QByteArray &payload);
    void handleRingPosition(const QByteArray &payload);
    void handleSessionRingMetadata(const QByteArray &payload);
    void handleSessionRingClips(const QByteArray &payload);

    QSerialPort m_serial;
    QByteArray m_rxBuffer;
    bool m_connected = false;
    ConnectionState m_connectionState = Disconnected;
    QString m_portName = QStringLiteral("/dev/serial0");
    int m_baudRate = 115200;
    QTimer m_reconnectTimer;
    QTimer m_trackCleanupTimer;
    ClipGridModel *m_clipModel = nullptr;
    TrackListModel *m_trackModel = nullptr;
    SceneListModel *m_sceneModel = nullptr;
    MixerModel *m_mixerModel = nullptr;
    bool m_transportPlaying = false;
    bool m_transportRecording = false;
    bool m_transportLoop = false;
    bool m_shiftPressed = false;
    double m_transportTempo = 120.0;
    QString m_transportPosition = QStringLiteral("1.1.1");
    int m_mixerMode = 0;  // 0=VOLUME_PAN, 1=SENDS_AB, 2=SENDS_CD, 3=MASTER_RETURNS
    int m_ringTrackOffset = 0;
    int m_ringSceneOffset = 0;
    QBitArray m_trackPresence;
    bool m_trackBatchSawZero = false;

    enum Cmd {
        CmdHandshake = 0x00,
        CmdHandshakeReply = 0x01,
        CmdDisconnect = 0x02,
        CmdPing = 0x03,
        CmdSelectedTrack = 0x06,
        CmdGridUpdate7bit = 0x60,
        CmdGridUpdate14bit = 0xA6, // CmdLedGridUpdate14
        CmdPadUpdate7bit = 0x84,   // CmdLedRgbState
        CmdPadUpdate14bit = 0xA7,  // CmdLedPadUpdate14
        CmdClipTrigger = 0x11,
        CmdClipName = 0x14,
        CmdClipState = 0x10,
        CmdTrackName = 0x27,
        CmdTrackColor = 0x28,
        CmdSceneName = 0x1B,
        CmdSceneColor = 0x1C,
        CmdSceneState = 0x1A,
        CmdSceneTriggered = 0x1D,
        CmdTransportPlay = 0x40,
        CmdTransportRecord = 0x41,
        CmdTransportLoop = 0x42,
        CmdTransportTempo = 0x43,
        CmdTransportPosition = 0x45,
        CmdTransportState = 0x49,
        CmdShiftState = 0x88,
        CmdRingPosition = 0x0C,  // Session ring position update
        // Mixer commands
        CmdMixerVolume = 0x21,
        CmdMixerPan = 0x22,
        CmdMixerMute = 0x23,
        CmdMixerSolo = 0x24,
        CmdMixerArm = 0x25,
        CmdMixerSend = 0x26,
        CmdTrackSelect = 0x0D,     // GUI → Teensy → Live: select track
        CmdMixerMode = 0x98,
        CmdMixerBankChange = 0x99,  // GUI → Teensy: notify bank change for fader pickup
        CmdSessionRingMetadata = 0x9A,  // Bulk session ring metadata (tracks/scenes names+colors)
        CmdSessionRingClips = 0x9B      // Bulk session ring clips (32 clips states+colors)
    };
};

#endif // SERIALCONTROLLER_H
