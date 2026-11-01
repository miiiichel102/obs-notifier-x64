#pragma once

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
enum class Corner { TopRight, TopLeft, BottomRight, BottomLeft };

struct Settings {
    bool enabled = true;
    bool showFilename = true;
    bool sound = false;
    double seconds = 3.5;
    int scale = 100;
    int margin = 24;
    Corner corner = Corner::TopRight;
    QString display = QStringLiteral("active");

    bool accepts(Kind kind) const;
    QJsonObject toJson() const;
    static Settings fromJson(const QJsonObject &json);
    static Settings load(const QString &path);
    bool save(const QString &path) const;
};
}
