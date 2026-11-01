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

QJsonObject Settings::toJson() const
{
    return {{"version", 2}, {"enabled", enabled},
            {"showFilename", showFilename}, {"sound", sound},
            {"seconds", seconds}, {"scale", scale}, {"margin", margin},
            {"corner", static_cast<int>(corner)}, {"display", display}};
}

Settings Settings::fromJson(const QJsonObject &j)
{
    Settings s;
    s.enabled = j.value("enabled").toBool(s.enabled);

    s.showFilename = j.value("showFilename").toBool(s.showFilename);
    s.sound = j.value("sound").toBool(s.sound);
    const double seconds = j.value("seconds").toDouble(s.seconds);
    s.seconds = std::isfinite(seconds) ? std::clamp(seconds, 1.0, 15.0) : s.seconds;
    s.scale = std::clamp(j.value("scale").toInt(s.scale), 75, 200);
    s.margin = std::clamp(j.value("margin").toInt(s.margin), 0, 160);
    const int corner = j.value("corner").toInt(0);
    s.corner = corner >= 0 && corner <= 3 ? static_cast<Corner>(corner) : Corner::TopRight;
    s.display = j.value("display").toString(s.display);
    if (s.display.isEmpty())
        s.display = QStringLiteral("active");
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
