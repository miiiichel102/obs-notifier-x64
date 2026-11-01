#pragma once

#include "settings.hpp"
#include <QElapsedTimer>
#include <QPalette>
#include <QPointer>
#include <QTimer>
#include <QWidget>

class QScreen;

namespace obs_notifier {
struct Notice {
    Kind kind;
    QString filename;
};

class Toast final : public QWidget {
public:
    explicit Toast(QWidget *obsWindow);
    ~Toast() override;
    void setSettings(const Settings &settings);
    void refreshTheme();
    void showNotice(Notice notice, bool preview = false);
    void dismissAll();
    bool busy() const { return isVisible(); }
    Kind currentKind() const { return current_.kind; }
    QImage snapshot() const;

protected:
    void paintEvent(QPaintEvent *) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void tick();
    void requestFrame();
    void place();
    QScreen *targetScreen() const;
    void drawCard(QPainter &painter) const;
    QPointer<QWidget> obsWindow_;
    Settings settings_;
    QPalette colors_;
    Notice current_{Kind::ReplayStarted, {}};
    QTimer holdTimer_;
    QElapsedTimer elapsed_;
    QElapsedTimer chimeElapsed_;
    QPoint anchor_;
    double effectiveScale_ = 1.0;
    int holdMs_ = 3500;
    bool animateEntry_ = true;
    bool animating_ = false;
    double fractionalX_ = 0.0;
};

QString noticeTitle(Kind kind);
QRect anchoredRect(const QRect &available, const QSize &size, Corner corner, int margin);
}
