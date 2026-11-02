#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>

namespace obs_notifier {
enum class Kind {
    ReplayStarted, ReplayStopped, ReplaySaved,
    StreamingStarted, StreamingStopped,
    RecordingStarted, RecordingStopped,
    VirtualCameraStarted, VirtualCameraStopped,
    ReplayAlreadyRunning, ReplayAlreadyStopped,
    StreamingAlreadyRunning, StreamingAlreadyStopped,
    RecordingAlreadyRunning, RecordingAlreadyStopped,
    VirtualCameraAlreadyRunning, VirtualCameraAlreadyStopped
};
enum class SoundEvent { Start, Stop, Saved };
enum class Corner { TopRight, TopLeft, BottomRight, BottomLeft };

struct Settings {
    bool enabled = true;
    bool showFilename = false;
    bool sound = false;          // master switch for all sounds
    bool soundStart = true;      // something starts, or is already running
    bool soundStop = true;       // something stops, or is already stopped
    bool soundSaved = true;      // a clip is saved
    QString soundStartFile;      // custom .wav; empty uses the built-in sound
    QString soundStopFile;
    QString soundSavedFile;
    double seconds = 2.0;
    int scale = 100;
    int margin = 24;
    Corner corner = Corner::TopRight;
    QString display = QStringLiteral("active");
    QString fontFamily;  // empty = default (Segoe UI Variable, then Segoe UI)
    int fontSize = 14;   // title size in pixels at 1080p; other text scales from it
    int titleWeight = 600;
    bool customColors = false; // false = follow the OBS theme
    QColor backgroundColor = QColor(0x1f, 0x1e, 0x25);
    QColor textColor = QColor(0xef, 0xef, 0xf2);
    QColor accentColor = QColor(0x47, 0x6b, 0xd7);

    bool accepts(Kind kind) const;
    bool soundEnabled(SoundEvent event) const;
    QString soundFile(SoundEvent event) const;
    QJsonObject toJson() const;
    static Settings fromJson(const QJsonObject &json);
    static Settings load(const QString &path);
    bool save(const QString &path) const;
};
}
