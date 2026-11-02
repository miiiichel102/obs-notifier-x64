#include "settings.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <cmath>

namespace obs_notifier {
bool Settings::accepts(Kind) const
{
    return enabled;
}

bool Settings::soundEnabled(SoundEvent event) const
{
    switch (event) {
    case SoundEvent::Start: return soundStart;
    case SoundEvent::Stop: return soundStop;
    case SoundEvent::Saved: return soundSaved;
    }
    return false;
}

QString Settings::soundFile(SoundEvent event) const
{
    switch (event) {
    case SoundEvent::Start: return soundStartFile;
    case SoundEvent::Stop: return soundStopFile;
    case SoundEvent::Saved: return soundSavedFile;
    }
    return {};
}

QJsonObject Settings::toJson() const
{
    return {{"version", 3}, {"enabled", enabled},
            {"showFilename", showFilename}, {"sound", sound},
            {"soundStart", soundStart}, {"soundStop", soundStop}, {"soundSaved", soundSaved},
            {"soundStartFile", soundStartFile}, {"soundStopFile", soundStopFile},
            {"soundSavedFile", soundSavedFile},
            {"seconds", seconds}, {"scale", scale}, {"margin", margin},
            {"corner", static_cast<int>(corner)}, {"display", display},
            {"fontFamily", fontFamily}, {"fontSize", fontSize}, {"titleWeight", titleWeight},
            {"customColors", customColors},
            {"backgroundColor", backgroundColor.name(QColor::HexArgb)},
            {"textColor", textColor.name(QColor::HexRgb)},
            {"accentColor", accentColor.name(QColor::HexRgb)}};
}

Settings Settings::fromJson(const QJsonObject &j)
{
    Settings s;
    s.enabled = j.value("enabled").toBool(s.enabled);

    s.showFilename = j.value("showFilename").toBool(s.showFilename);
    s.sound = j.value("sound").toBool(s.sound);
    s.soundStart = j.value("soundStart").toBool(s.soundStart);
    s.soundStop = j.value("soundStop").toBool(s.soundStop);
    s.soundSaved = j.value("soundSaved").toBool(s.soundSaved);
    s.soundStartFile = j.value("soundStartFile").toString().trimmed();
    s.soundStopFile = j.value("soundStopFile").toString().trimmed();
    s.soundSavedFile = j.value("soundSavedFile").toString().trimmed();
    const double seconds = j.value("seconds").toDouble(s.seconds);
    s.seconds = std::isfinite(seconds) ? std::clamp(seconds, 1.0, 15.0) : s.seconds;
    s.scale = std::clamp(j.value("scale").toInt(s.scale), 75, 200);
    s.margin = std::clamp(j.value("margin").toInt(s.margin), 0, 160);
    const int corner = j.value("corner").toInt(0);
    s.corner = corner >= 0 && corner <= 3 ? static_cast<Corner>(corner) : Corner::TopRight;
    s.display = j.value("display").toString(s.display);
    if (s.display.isEmpty())
        s.display = QStringLiteral("active");
    s.fontFamily = j.value("fontFamily").toString(s.fontFamily).trimmed();
    s.fontSize = std::clamp(j.value("fontSize").toInt(s.fontSize), 10, 28);
    const int weight = j.value("titleWeight").toInt(s.titleWeight);
    s.customColors = j.value("customColors").toBool(s.customColors);
    const auto readColor = [&j](const char *key, QColor fallback, bool alpha) {
        QColor color = QColor::fromString(j.value(key).toString());
        if (!color.isValid())
            return fallback;
        if (!alpha)
            color.setAlpha(255);
        return color;
    };
    s.backgroundColor = readColor("backgroundColor", s.backgroundColor, true);
    s.textColor = readColor("textColor", s.textColor, false);
    s.accentColor = readColor("accentColor", s.accentColor, false);
    s.titleWeight = weight == 400 || weight == 500 || weight == 600 || weight == 700 ? weight : 600;
    return s;
}

Settings Settings::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return fromJson(QJsonDocument::fromJson(file.readAll()).object());
}

bool Settings::save(const QString &path) const
{
    if (path.isEmpty() || !QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const auto json = QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size())
        return false;
    return file.commit();
}
}
