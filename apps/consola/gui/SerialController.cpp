#include "SerialController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QColor>
#include <QtMath>

namespace {

int decode14Bit(quint8 msb, quint8 lsb)
{
    return ((msb & 0x7F) << 7) | (lsb & 0x7F);
}

int normalize14To8(quint8 msb, quint8 lsb)
{
    // Colors are packed as two 7-bit MIDI-safe bytes that together represent
    // the original 0-255 component. Just reconstruct and clamp.
    return qBound(0, decode14Bit(msb, lsb), 255);
}

int normalize7To8(quint8 value)
{
    return static_cast<int>(((value & 0x7F) * 255) / 127);
}

QColor colorFrom14(const quint8 *data)
{
    return QColor(normalize14To8(data[0], data[1]),
                  normalize14To8(data[2], data[3]),
                  normalize14To8(data[4], data[5]));
}

QColor colorFrom7(const quint8 *data)
{
    return QColor(normalize7To8(data[0]),
                  normalize7To8(data[1]),
                  normalize7To8(data[2]));
}

QString readLengthPrefixedString(const QByteArray &payload, int offset)
{
    if (offset >= payload.size())
        return QString();

    const int remaining = payload.size() - offset;
    if (remaining <= 0)
        return QString();

    const quint8 declaredLen = static_cast<quint8>(payload.at(offset));
    const int available = remaining - 1;
    if (declaredLen > 0 && available >= declaredLen)
        return QString::fromUtf8(payload.constData() + offset + 1, declaredLen);

    if (available <= 0)
        return QString();

    // Fallback: treat the rest as a raw UTF-8 string (legacy packets)
    return QString::fromUtf8(payload.constData() + offset, remaining);
}
}
constexpr quint8 FrameHeader = 0xAA;

SerialController::SerialController(QObject *parent)
    : QObject(parent)
    , m_clipModel(new ClipGridModel(this))
    , m_trackModel(new TrackListModel(this))
    , m_sceneModel(new SceneListModel(this))
    , m_mixerModel(new MixerModel(this))
{
    connect(&m_serial, &QSerialPort::readyRead, this, &SerialController::handleReadyRead);
    connect(&m_serial, &QSerialPort::errorOccurred, this, &SerialController::handleError);

    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(2000);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &SerialController::handleReconnectTimeout);

    m_trackCleanupTimer.setSingleShot(true);
    m_trackCleanupTimer.setInterval(100);
    connect(&m_trackCleanupTimer, &QTimer::timeout, this, &SerialController::handleTrackBatchTimeout);

    m_trackPresence.resize(8);
    m_trackPresence.fill(false);

    openPort();
}

void SerialController::setPortName(const QString &name)
{
    if (m_portName == name)
        return;

    m_portName = name;
    emit portNameChanged();
    reconnect();
}

void SerialController::setBaudRate(int baud)
{
    if (m_baudRate == baud)
        return;

    m_baudRate = baud;
    emit baudRateChanged();
    reconnect();
}

void SerialController::reconnect()
{
    closePort();
    openPort();
}

void SerialController::requestDisconnect()
{
    if (m_serial.isOpen()) {
        qInfo() << "Solicitando desconexión (CMD_DISCONNECT)";
        sendFrame(CmdDisconnect);
    }
    setConnected(false);
    setConnectionState(WaitingHandshake);
}

void SerialController::sendTransportPlay(bool state)
{
    QByteArray payload(1, char(state ? 1 : 0));
    sendFrame(CmdTransportPlay, payload);
}

void SerialController::sendTransportRecord(bool state)
{
    QByteArray payload(1, char(state ? 1 : 0));
    sendFrame(CmdTransportRecord, payload);
}

void SerialController::sendTransportLoop(bool state)
{
    QByteArray payload(1, char(state ? 1 : 0));
    sendFrame(CmdTransportLoop, payload);
}

void SerialController::sendClipTrigger(int track, int scene)
{
    QByteArray payload;
    payload.append(static_cast<char>(track));
    payload.append(static_cast<char>(scene));
    sendFrame(CmdClipTrigger, payload);
}

void SerialController::sendMixerBankChange(int bank)
{
    qDebug() << "📤 Sending mixer bank change to Teensy: bank" << bank;
    QByteArray payload;
    payload.append(static_cast<char>(bank & 0x7F));
    sendFrame(CmdMixerBankChange, payload);
}

void SerialController::sendTrackSelect(int trackIndex)
{
    qDebug() << "📤 Sending track select to Teensy: track" << trackIndex;
    QByteArray payload;
    payload.append(static_cast<char>(trackIndex & 0x7F));
    sendFrame(CmdTrackSelect, payload);
}

void SerialController::openPort()
{
    if (m_serial.isOpen())
        return;

    m_serial.setPortName(m_portName);
    m_serial.setBaudRate(m_baudRate);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial.open(QIODevice::ReadWrite)) {
        const QString message = tr("Unable to open %1: %2").arg(m_portName, m_serial.errorString());
        qWarning() << message;
        emit connectionError(message);
        if (!m_reconnectTimer.isActive())
            m_reconnectTimer.start();
        return;
    }

    qInfo() << "Serial port opened on" << m_portName << "@" << m_baudRate;
    setConnectionState(WaitingHandshake);
}

void SerialController::closePort()
{
    if (!m_serial.isOpen())
        return;

    m_serial.close();
    setConnected(false);
    setConnectionState(Disconnected);
    m_rxBuffer.clear();
    m_trackCleanupTimer.stop();
    m_trackBatchSawZero = false;
    m_trackPresence.fill(false);
    if (m_trackModel)
        m_trackModel->resetAll();
}

void SerialController::handleReadyRead()
{
    const QByteArray chunk = m_serial.readAll();
    if (!chunk.isEmpty()) {
        qInfo().noquote() << QStringLiteral("[RX] RAW %1")
                             .arg(QString::fromLatin1(chunk.toHex(' ')));
    }
    m_rxBuffer.append(chunk);

    while (true) {
        const int syncIndex = m_rxBuffer.indexOf(char(FrameHeader));
        if (syncIndex < 0) {
            // No byte de sincronización en el buffer -> descartar ruido
            if (!m_rxBuffer.isEmpty())
                qWarning() << "Descartando" << m_rxBuffer.size() << "bytes sin SYNC";
            m_rxBuffer.clear();
            break;
        }

        if (syncIndex > 0) {
            // Quitar bytes previos hasta el siguiente SYNC
            m_rxBuffer.remove(0, syncIndex);
        }

        if (m_rxBuffer.size() < 3)
            break;

        const quint8 cmd = quint8(m_rxBuffer.at(1));
        const quint8 len = quint8(m_rxBuffer.at(2));

        const int totalSize = 3 + len + 1;
        if (m_rxBuffer.size() < totalSize)
            return; // wait for more data

        const QByteArray payload = m_rxBuffer.mid(3, len);
        const quint8 checksum = quint8(m_rxBuffer.at(3 + len));
        if (checksum != calculateChecksum(cmd, len, payload)) {
            qWarning() << "Checksum mismatch for cmd" << Qt::hex << cmd
                       << "len" << len
                       << "payload" << payload.toHex(' ');
            // Reiniciar parser desde el próximo SYNC
            m_rxBuffer.remove(0, 1);
            continue;
        }

        qInfo().noquote() << QStringLiteral("[RX] FRAME cmd=0x%1 len=%2 payload=%3")
                             .arg(cmd, 2, 16, QLatin1Char('0'))
                             .arg(len)
                             .arg(QString::fromLatin1(payload.toHex(' ')));
        processFrame(cmd, payload);
        m_rxBuffer.remove(0, totalSize);
        // Reset parser state before continuar
        continue;
    }
}

void SerialController::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    const QString message = tr("Serial error (%1): %2").arg(int(error)).arg(m_serial.errorString());
    qWarning() << message;
    emit connectionError(message);
    closePort();

    if (!m_reconnectTimer.isActive())
        m_reconnectTimer.start();
}

void SerialController::handleReconnectTimeout()
{
    if (!m_serial.isOpen())
        openPort();
}

void SerialController::processFrame(quint8 cmd, const QByteArray &payload)
{
    // Log ALL commands to see if mixer commands reach the switch
    if (cmd == 0x21) {
        qWarning() << "🎯🎯🎯 processFrame: CMD=0x21 (CmdMixerVolume) payload.size()=" << payload.size();
    }

    switch (cmd) {
    case CmdHandshake:
        if (payload == QByteArrayLiteral("PUSHCLONE_GUI")) {
            qInfo() << "Handshake recibido";
            setConnected(true);
            setConnectionState(Connected);
            sendFrame(CmdHandshakeReply, QByteArrayLiteral("PUSHCLONE_GUI"));
        } else {
            qWarning() << "Handshake payload inesperado" << payload;
        }
        break;
    case CmdPing:
        sendFrame(CmdPing);
        break;
    case CmdDisconnect:
        qInfo() << "CMD_DISCONNECT recibido";
        setConnected(false);
        setConnectionState(WaitingHandshake);
        break;
    case CmdSelectedTrack:
        if (payload.size() >= 1) {
            int trackIndex = payload.at(0) & 0x7F;
            qDebug() << "🎯 Selected track changed:" << trackIndex;
            if (m_mixerModel) {
                m_mixerModel->setSelectedTrackIndex(trackIndex);
            }
        }
        break;
    case CmdClipName:
        handleClipName(payload);
        break;
    case CmdGridUpdate7bit:
        handleGridUpdate7bit(payload);
        break;
    case CmdGridUpdate14bit:
        handleGridUpdate14bit(payload);
        break;
    case CmdPadUpdate14bit:
        handlePadUpdate14bit(payload);
        break;
    case CmdPadUpdate7bit:
        handlePadUpdate7bit(payload);
        break;
    case CmdClipState:
        handleClipState(payload);
        break;
    case CmdTrackName:
        handleTrackName(payload);
        break;
    case CmdTrackColor:
        handleTrackColor(payload);
        break;
    case CmdSceneName:
        handleSceneName(payload);
        break;
    case CmdSceneColor:
        handleSceneColor(payload);
        break;
    case CmdSceneTriggered:
    case CmdSceneState:
        handleSceneTriggered(payload);
        break;
    case CmdTransportPlay:
    case CmdTransportRecord:
    case CmdTransportLoop:
    case CmdTransportTempo:
    case CmdTransportState:
        handleTransportCommand(cmd, payload);
        break;

    case CmdShiftState:
        if (payload.size() >= 1) {
            bool pressed = (payload[0] != 0);
            if (m_shiftPressed != pressed) {
                m_shiftPressed = pressed;
                emit shiftPressedChanged();
            }
        }
        break;
    
    // Mixer commands
    case CmdMixerVolume:
        qWarning() << "⚡⚡⚡ CASE CmdMixerVolume REACHED!";
        handleMixerVolume(payload);
        break;
    case CmdMixerPan:
        handleMixerPan(payload);
        break;
    case CmdMixerMute:
        handleMixerMute(payload);
        break;
    case CmdMixerSolo:
        handleMixerSolo(payload);
        break;
    case CmdMixerArm:
        handleMixerArm(payload);
        break;
    case CmdMixerSend:
        handleMixerSend(payload);
        break;
    case CmdMixerMode:
        handleMixerMode(payload);
        break;
    case CmdRingPosition:
        handleRingPosition(payload);
        break;
    case CmdSessionRingMetadata:
        handleSessionRingMetadata(payload);
        break;
    case CmdSessionRingClips:
        handleSessionRingClips(payload);
        break;

    default:
        // Por ahora solo registramos otros comandos para depuración.
        qDebug() << "Frame recibido:" << Qt::hex << cmd << "len" << payload.size();
        break;
    }
}

void SerialController::sendFrame(quint8 cmd, const QByteArray &payload)
{
    if (!m_serial.isOpen()) {
        qWarning() << "No serial port open to send frame";
        return;
    }

    const quint8 len = static_cast<quint8>(payload.size());
    QByteArray frame;
    frame.reserve(3 + payload.size() + 1);
    frame.append(char(FrameHeader));
    frame.append(char(cmd));
    frame.append(char(len));
    frame.append(payload);
    frame.append(char(calculateChecksum(cmd, len, payload)));

    qInfo().noquote() << QStringLiteral("[TX] FRAME cmd=0x%1 len=%2 payload=%3")
                         .arg(cmd, 2, 16, QLatin1Char('0'))
                         .arg(len)
                         .arg(QString::fromLatin1(payload.toHex(' ')));

    const qint64 written = m_serial.write(frame);
    if (written != frame.size()) {
        qWarning() << "Failed to write complete frame";
    }
    m_serial.flush();

    // Opcional: volcar los bytes exactos que salieron (incluyendo header y checksum)
    qInfo().noquote() << QStringLiteral("[TX] RAW %1")
                         .arg(QString::fromLatin1(frame.toHex(' ')));
}

quint8 SerialController::calculateChecksum(quint8 cmd, quint8 len, const QByteArray &payload) const
{
    quint8 checksum = cmd ^ len;
    for (const char byte : payload)
        checksum ^= quint8(byte);
    return checksum;
}

void SerialController::setConnected(bool value)
{
    if (m_connected == value)
        return;

    m_connected = value;
    emit connectedChanged();
}

void SerialController::setConnectionState(ConnectionState state)
{
    if (m_connectionState == state)
        return;
    m_connectionState = state;
    emit connectionStateChanged();

}

void SerialController::handleClipName(const QByteArray &payload)
{
    if (payload.size() < 2 || !m_clipModel)
        return;
    const int absoluteTrack = static_cast<quint8>(payload.at(0));
    const int absoluteScene = static_cast<quint8>(payload.at(1));
    const QString name = readLengthPrefixedString(payload, 2);

    // Convert to relative indices
    const int relativeTrack = absoluteTrack - m_ringTrackOffset;
    const int relativeScene = absoluteScene - m_ringSceneOffset;

    // Only update if clip is within visible session ring (8x4)
    if (relativeTrack >= 0 && relativeTrack < 8 &&
        relativeScene >= 0 && relativeScene < 4) {
        m_clipModel->setClipName(relativeTrack, relativeScene, name);
    }
}

void SerialController::handleGridUpdate7bit(const QByteArray &payload)
{
    if (!m_clipModel || payload.size() < 3)
        return;
    const int padCount = payload.size() / 3;
    const int totalRows = m_clipModel->rowCount();
    for (int i = 0; i < padCount && i < totalRows; ++i) {
        const quint8 *colorData = reinterpret_cast<const quint8 *>(payload.constData() + i * 3);
        updatePadColor(i % 8, i / 8, colorFrom7(colorData));
    }
}

void SerialController::handleGridUpdate14bit(const QByteArray &payload)
{
    if (!m_clipModel || payload.size() < 6)
        return;
    const int padCount = payload.size() / 6;
    const int totalRows = m_clipModel->rowCount();
    for (int i = 0; i < padCount && i < totalRows; ++i) {
        const quint8 *colorData = reinterpret_cast<const quint8 *>(payload.constData() + i * 6);
        updatePadColor(i % 8, i / 8, colorFrom14(colorData));
    }
}

void SerialController::handlePadUpdate14bit(const QByteArray &payload)
{
    if (!m_clipModel || payload.size() < 7)
        return;

    int track = -1;
    int scene = -1;
    int offset = 1;

    if (payload.size() >= 8 &&
        static_cast<quint8>(payload.at(0)) < 8 &&
        static_cast<quint8>(payload.at(1)) < 4) {
        track = static_cast<quint8>(payload.at(0));
        scene = static_cast<quint8>(payload.at(1));
        offset = 2;
    } else {
        const int padIndex = static_cast<quint8>(payload.at(0));
        track = padIndex % 8;
        scene = padIndex / 8;
    }

    if (payload.size() < offset + 6)
        return;

    const quint8 *colorData = reinterpret_cast<const quint8 *>(payload.constData() + offset);
    updatePadColor(track, scene, colorFrom14(colorData));
}

void SerialController::handlePadUpdate7bit(const QByteArray &payload)
{
    if (!m_clipModel || payload.size() < 5)
        return;

    const int absoluteTrack = static_cast<quint8>(payload.at(0));
    const int absoluteScene = static_cast<quint8>(payload.at(1));
    const quint8 *colorData = reinterpret_cast<const quint8 *>(payload.constData() + 2);

    // Convert to relative indices
    const int relativeTrack = absoluteTrack - m_ringTrackOffset;
    const int relativeScene = absoluteScene - m_ringSceneOffset;

    // Only update if clip is within visible session ring (8x4)
    if (relativeTrack >= 0 && relativeTrack < 8 &&
        relativeScene >= 0 && relativeScene < 4) {
        updatePadColor(relativeTrack, relativeScene, colorFrom7(colorData));
    }
}

void SerialController::handleClipState(const QByteArray &payload)
{
    if (!m_clipModel || payload.size() < 3)
        return;
    const int absoluteTrack = static_cast<quint8>(payload.at(0));
    const int absoluteScene = static_cast<quint8>(payload.at(1));
    const int state = static_cast<quint8>(payload.at(2));

    // Convert to relative indices
    const int relativeTrack = absoluteTrack - m_ringTrackOffset;
    const int relativeScene = absoluteScene - m_ringSceneOffset;

    // Only update if clip is within visible session ring (8x4)
    if (relativeTrack >= 0 && relativeTrack < 8 &&
        relativeScene >= 0 && relativeScene < 4) {
        m_clipModel->setClipState(relativeTrack, relativeScene, state);

        if (payload.size() >= 9) {
            const quint8 *colorData = reinterpret_cast<const quint8 *>(payload.constData() + 3);
            updatePadColor(relativeTrack, relativeScene, colorFrom14(colorData));
        }
    }
}

void SerialController::updatePadColor(int track, int scene, const QColor &color)
{
    if (!m_clipModel)
        return;
    // This method now expects RELATIVE indices (already converted by callers)
    m_clipModel->setClipColor(track, scene, color);
}

void SerialController::handleTrackName(const QByteArray &payload)
{
    if (payload.size() < 1)
        return;
    const int absoluteTrack = static_cast<quint8>(payload.at(0));
    const QString name = readLengthPrefixedString(payload, 1);

    // Convert absolute track index to relative (based on session ring offset)
    const int relativeTrack = absoluteTrack - m_ringTrackOffset;

    // Only update if track is within visible session ring (0-7)
    if (relativeTrack >= 0 && relativeTrack < 8) {
        if (m_trackModel)
            m_trackModel->setTrackName(relativeTrack, name);
    }

    // MixerModel uses absolute indices (8 tracks total, independent of ring)
    if (m_mixerModel)
        m_mixerModel->setTrackName(absoluteTrack, name);

    scheduleTrackCleanup(absoluteTrack);
}

void SerialController::handleTrackColor(const QByteArray &payload)
{
    if (payload.size() < 4)
        return;
    const int absoluteTrack = static_cast<quint8>(payload.at(0));
    const quint8 *colorData = reinterpret_cast<const quint8 *>(payload.constData() + 1);
    QColor color = (payload.size() >= 7) ? colorFrom14(colorData)
                                         : colorFrom7(colorData);

    // Convert absolute track index to relative (based on session ring offset)
    const int relativeTrack = absoluteTrack - m_ringTrackOffset;

    // Only update if track is within visible session ring (0-7)
    if (relativeTrack >= 0 && relativeTrack < 8) {
        if (m_trackModel)
            m_trackModel->setTrackColor(relativeTrack, color);
    }

    // MixerModel uses absolute indices
    if (m_mixerModel)
        m_mixerModel->setTrackColor(absoluteTrack, color);
}

void SerialController::handleSceneName(const QByteArray &payload)
{
    if (!m_sceneModel || payload.size() < 1)
        return;
    const int scene = static_cast<quint8>(payload.at(0));
    const QString name = QString::fromUtf8(payload.constData() + 1, payload.size() - 1);
    m_sceneModel->setSceneName(scene, name);
}

void SerialController::handleSceneColor(const QByteArray &payload)
{
    if (!m_sceneModel || payload.size() < 4)
        return;
    const int scene = static_cast<quint8>(payload.at(0));
    const quint8 *colorData = reinterpret_cast<const quint8 *>(payload.constData() + 1);
    QColor color = (payload.size() >= 7) ? colorFrom14(colorData)
                                         : colorFrom7(colorData);
    m_sceneModel->setSceneColor(scene, color);
}

void SerialController::handleSceneTriggered(const QByteArray &payload)
{
    if (!m_sceneModel || payload.size() < 2)
        return;
    const int scene = static_cast<quint8>(payload.at(0));
    const bool triggered = static_cast<quint8>(payload.at(1)) != 0;
    m_sceneModel->setSceneTriggered(scene, triggered);
}

void SerialController::handleTransportCommand(quint8 cmd, const QByteArray &payload)
{
    auto boolValue = [&](const QByteArray &data) -> bool {
        if (data.isEmpty())
            return false;
        return static_cast<quint8>(data.at(0)) != 0;
    };

    switch (cmd) {
    case CmdTransportPlay:
        if (m_transportPlaying != boolValue(payload)) {
            m_transportPlaying = boolValue(payload);
            emit transportStateChanged();
        }
        break;
    case CmdTransportRecord:
        if (m_transportRecording != boolValue(payload)) {
            m_transportRecording = boolValue(payload);
            emit transportStateChanged();
        }
        break;
    case CmdTransportLoop:
        if (m_transportLoop != boolValue(payload)) {
            m_transportLoop = boolValue(payload);
            emit transportStateChanged();
        }
        break;
    case CmdTransportTempo:
        if (payload.size() >= 2) {
            // BPM is sent as 14-bit value (like mixer params)
            const quint8 msb = payload.at(0) & 0x7F;
            const quint8 lsb = payload.at(1) & 0x7F;
            const int value14bit = decode14Bit(msb, lsb);
            const double tempo = value14bit / 10.0;

            qWarning() << "🎼 BPM Debug: MSB=" << msb << "LSB=" << lsb
                      << "14bit=" << value14bit << "BPM=" << tempo;

            if (!qFuzzyCompare(tempo, m_transportTempo)) {
                m_transportTempo = tempo;
                emit transportTempoChanged();
            }
        }
        break;
    case CmdTransportPosition:
        if (!payload.isEmpty()) {
            const QString position = QString::fromUtf8(payload);
            if (m_transportPosition != position) {
                m_transportPosition = position;
                emit transportPositionChanged();
            }
        }
        break;
    case CmdTransportState:
        if (payload.size() >= 1) {
            const quint8 flags = static_cast<quint8>(payload.at(0));
            bool playing = flags & 0x01;
            bool recording = flags & 0x02;
            bool loop = flags & 0x04;
            bool changed = false;
            if (m_transportPlaying != playing) {
                m_transportPlaying = playing;
                changed = true;
            }
            if (m_transportRecording != recording) {
                m_transportRecording = recording;
                changed = true;
            }
            if (m_transportLoop != loop) {
                m_transportLoop = loop;
                changed = true;
            }
            if (changed)
                emit transportStateChanged();
        }
        break;
    default:
        break;
    }
}

void SerialController::scheduleTrackCleanup(int trackIndex)
{
    if (trackIndex >= 0 && trackIndex < m_trackPresence.size())
        m_trackPresence.setBit(trackIndex, true);

    if (trackIndex == 0)
        m_trackBatchSawZero = true;

    m_trackCleanupTimer.start();
}

void SerialController::handleTrackBatchTimeout()
{
    if (!m_trackModel)
        return;

    if (m_trackBatchSawZero) {
        int lastContiguous = -1;
        for (int i = 0; i < m_trackPresence.size(); ++i) {
            if (!m_trackPresence.testBit(i))
                break;
            lastContiguous = i;
        }
        m_trackModel->clearAbove(lastContiguous);
    }

    m_trackPresence.fill(false);
    m_trackBatchSawZero = false;
}

// ═══════════════════════════════════════════════════════════
// MIXER HANDLERS
// ═══════════════════════════════════════════════════════════

void SerialController::handleMixerVolume(const QByteArray &payload)
{
    qWarning() << "🔊🔊🔊 handleMixerVolume CALLED - payload size:" << payload.size();

    if (!m_mixerModel) {
        qWarning() << "❌ handleMixerVolume: m_mixerModel is NULL!";
        return;
    }

    if (payload.size() < 3) {
        qWarning() << "❌ handleMixerVolume: payload too short:" << payload.size();
        return;
    }

    const quint8 trackIndex = payload[0] & 0x7F;
    const quint8 valueMsb = payload[1] & 0x7F;
    const quint8 valueLsb = payload[2] & 0x7F;

    qWarning() << "   Parsed: track=" << trackIndex << "MSB=" << valueMsb << "LSB=" << valueLsb;

    // Decode 14-bit value to 0.0-1.0
    const int value14bit = decode14Bit(valueMsb, valueLsb);
    const float volume = qBound(0.0f, value14bit / 16383.0f, 1.0f);

    qWarning() << "   Decoded: 14bit=" << value14bit << "volume=" << volume;
    qWarning() << "   MixerModel totalTracks=" << m_mixerModel->totalTracks();

    m_mixerModel->setTrackVolume(trackIndex, volume);

    qWarning() << "✅ Mixer Volume:" << trackIndex << "→" << volume;
}

void SerialController::handleMixerPan(const QByteArray &payload)
{
    if (!m_mixerModel || payload.size() < 3)
        return;

    const quint8 trackIndex = payload[0] & 0x7F;
    const quint8 valueMsb = payload[1] & 0x7F;
    const quint8 valueLsb = payload[2] & 0x7F;
    
    // Decode 14-bit value to 0.0-1.0 (0.5 = center)
    const int value14bit = decode14Bit(valueMsb, valueLsb);
    const float pan = qBound(0.0f, value14bit / 16383.0f, 1.0f);
    
    m_mixerModel->setTrackPan(trackIndex, pan);
    
    qDebug() << "Mixer Pan:" << trackIndex << "→" << pan;
}

void SerialController::handleMixerMute(const QByteArray &payload)
{
    if (!m_mixerModel || payload.size() < 2)
        return;

    const quint8 trackIndex = payload[0] & 0x7F;
    const bool muted = (payload[1] & 0x7F) != 0;
    
    m_mixerModel->setTrackMuted(trackIndex, muted);
    
    qDebug() << "Mixer Mute:" << trackIndex << "→" << muted;
}

void SerialController::handleMixerSolo(const QByteArray &payload)
{
    if (!m_mixerModel || payload.size() < 2)
        return;

    const quint8 trackIndex = payload[0] & 0x7F;
    const bool solo = (payload[1] & 0x7F) != 0;
    
    m_mixerModel->setTrackSolo(trackIndex, solo);
    
    qDebug() << "Mixer Solo:" << trackIndex << "→" << solo;
}

void SerialController::handleMixerArm(const QByteArray &payload)
{
    if (!m_mixerModel || payload.size() < 2)
        return;

    const quint8 trackIndex = payload[0] & 0x7F;
    const bool armed = (payload[1] & 0x7F) != 0;
    
    m_mixerModel->setTrackArmed(trackIndex, armed);
    
    qDebug() << "Mixer Arm:" << trackIndex << "→" << armed;
}

void SerialController::handleMixerSend(const QByteArray &payload)
{
    if (!m_mixerModel || payload.size() < 4)
        return;

    const quint8 trackIndex = payload[0] & 0x7F;
    const quint8 sendIndex = payload[1] & 0x7F;  // 0=SendA, 1=SendB, etc.
    const quint8 valueMsb = payload[2] & 0x7F;
    const quint8 valueLsb = payload[3] & 0x7F;

    // Decode 14-bit value to 0.0-1.0
    const int value14bit = decode14Bit(valueMsb, valueLsb);
    const float sendLevel = qBound(0.0f, value14bit / 16383.0f, 1.0f);

    m_mixerModel->setTrackSend(trackIndex, sendIndex, sendLevel);

    qDebug() << "Mixer Send:" << trackIndex << "Send" << sendIndex << "→" << sendLevel;
}

void SerialController::handleMixerMode(const QByteArray &payload)
{
    if (payload.size() < 1)
        return;

    const quint8 mode = payload[0] & 0x7F;

    if (m_mixerMode != mode) {
        m_mixerMode = mode;

        // Notify QML about mixer mode change
        // 0=VOLUME_PAN, 1=SENDS_AB, 2=SENDS_CD, 3=MASTER_RETURNS
        emit mixerModeChanged(mode);

        qDebug() << "Mixer Mode changed to:" << mode;
    }
}

void SerialController::handleRingPosition(const QByteArray &payload)
{
    // Payload: [track_msb, track_lsb, scene_msb, scene_lsb, width, height, overview]
    if (payload.size() < 7)
        return;

    const int trackOffset = ((payload[0] & 0x7F) << 7) | (payload[1] & 0x7F);
    const int sceneOffset = ((payload[2] & 0x7F) << 7) | (payload[3] & 0x7F);
    const int width = payload[4] & 0x7F;
    const int height = payload[5] & 0x7F;

    if (m_ringTrackOffset != trackOffset || m_ringSceneOffset != sceneOffset) {
        qDebug() << "📍 Session ring moved:" << m_ringTrackOffset << "→" << trackOffset
                 << "," << m_ringSceneOffset << "→" << sceneOffset;

        m_ringTrackOffset = trackOffset;
        m_ringSceneOffset = sceneOffset;

        // Clear models when ring position changes so new data can populate
        if (m_trackModel) {
            m_trackModel->resetAll();
            qDebug() << "   ↳ TrackListModel cleared";
        }
        if (m_clipModel) {
            m_clipModel->resetAll(QColor("#1a1a1a"));  // Reset to dark gray
            qDebug() << "   ↳ ClipGridModel cleared";
        }

        emit ringPositionChanged();

        qDebug() << "📍 Session ring position:" << trackOffset << sceneOffset
                 << QString("(%1x%2)").arg(width).arg(height);
    }
}

void SerialController::handleSessionRingMetadata(const QByteArray &payload)
{
    // Bulk metadata: tracks and scenes with names and colors
    // Format: [num_tracks] [track0: len, name..., R, G, B] ... [track7: ...]
    //         [num_scenes] [scene0: len, name..., R, G, B] ... [scene3: ...]

    int offset = 0;
    if (payload.size() < 1) {
        qWarning() << "Ring metadata bulk payload too short";
        return;
    }

    // Parse tracks
    quint8 numTracks = payload[offset++] & 0x7F;
    qDebug() << "📦 Ring metadata bulk:" << numTracks << "tracks";

    for (quint8 t = 0; t < numTracks && offset < payload.size(); t++) {
        if (offset >= payload.size()) break;

        quint8 nameLen = payload[offset++] & 0x7F;
        if (offset + nameLen + 3 > payload.size()) break;  // name + RGB

        QString trackName;
        for (quint8 i = 0; i < nameLen; i++) {
            trackName.append(QChar(payload[offset++] & 0x7F));
        }

        quint8 r7 = payload[offset++] & 0x7F;
        quint8 g7 = payload[offset++] & 0x7F;
        quint8 b7 = payload[offset++] & 0x7F;

        // Convert 7-bit to 8-bit
        quint8 r8 = r7 << 1;
        quint8 g8 = g7 << 1;
        quint8 b8 = b7 << 1;

        QColor trackColor(r8, g8, b8);

        if (m_trackModel) {
            m_trackModel->setTrackName(t, trackName);
            m_trackModel->setTrackColor(t, trackColor);
        }
    }

    // Clear tracks above the received count (handles track deletion)
    if (m_trackModel && numTracks > 0) {
        m_trackModel->clearAbove(numTracks - 1);
    }

    // Parse scenes
    if (offset < payload.size()) {
        quint8 numScenes = payload[offset++] & 0x7F;
        qDebug() << "📦 Ring metadata bulk:" << numScenes << "scenes";

        for (quint8 s = 0; s < numScenes && offset < payload.size(); s++) {
            if (offset >= payload.size()) break;

            quint8 nameLen = payload[offset++] & 0x7F;
            if (offset + nameLen + 3 > payload.size()) break;

            QString sceneName;
            for (quint8 i = 0; i < nameLen; i++) {
                sceneName.append(QChar(payload[offset++] & 0x7F));
            }

            quint8 r7 = payload[offset++] & 0x7F;
            quint8 g7 = payload[offset++] & 0x7F;
            quint8 b7 = payload[offset++] & 0x7F;

            // Convert 7-bit to 8-bit
            quint8 r8 = r7 << 1;
            quint8 g8 = g7 << 1;
            quint8 b8 = b7 << 1;

            QColor sceneColor(r8, g8, b8);

            if (m_sceneModel) {
                m_sceneModel->setSceneName(s, sceneName);
                m_sceneModel->setSceneColor(s, sceneColor);
            }
        }

        // Clear scenes above the received count (handles scene deletion)
        if (m_sceneModel && numScenes > 0) {
            m_sceneModel->clearAbove(numScenes - 1);
        }
    }

    qDebug() << "✅ Processed ring metadata bulk (" << payload.size() << "bytes)";
}

void SerialController::handleSessionRingClips(const QByteArray &payload)
{
    // Bulk clips: 32 clips with states and colors
    // Format: [clip0: state, R, G, B] [clip1: ...] ... [clip31: ...]
    // Order: column-major (track 0 scenes 0-3, track 1 scenes 0-3, ...)

    const int expectedBytes = 32 * 4;  // 32 clips × 4 bytes each
    if (payload.size() < expectedBytes) {
        qWarning() << "Ring clips bulk payload too short (got" << payload.size()
                   << ", need" << expectedBytes << ")";
        return;
    }

    qDebug() << "📦 Ring clips bulk: 32 clips";

    int offset = 0;
    for (int track = 0; track < 8; track++) {
        for (int scene = 0; scene < 4; scene++) {
            if (offset + 3 >= payload.size()) break;

            quint8 state = payload[offset++] & 0x7F;
            quint8 r7 = payload[offset++] & 0x7F;
            quint8 g7 = payload[offset++] & 0x7F;
            quint8 b7 = payload[offset++] & 0x7F;

            // Convert 7-bit to 8-bit for RGB
            quint8 r8 = r7 << 1;
            quint8 g8 = g7 << 1;
            quint8 b8 = b7 << 1;

            QColor clipColor(r8, g8, b8);

            if (m_clipModel) {
                m_clipModel->setClipState(track, scene, state);
                m_clipModel->setClipColor(track, scene, clipColor);
            }
        }
    }

    qDebug() << "✅ Processed ring clips bulk (32 clips)";
}
