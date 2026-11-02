#pragma once

#include "settings.hpp"
#include <QElapsedTimer>
#include <QImage>
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
    QSizeF cardSize() const;
    void drawCard(QPainter &painter, const QSizeF &size) const;
    QPointer<QWidget> obsWindow_;
    Settings settings_;
    QPalette colors_;
    Notice current_{Kind::ReplayStarted, {}};
    QTimer holdTimer_;
    QElapsedTimer elapsed_;
    QElapsedTimer chimeElapsed_;
    QPoint anchor_;
    QSizeF cardSize_;
    QImage cardImage_;              // card rendered once per notice, then only moved and faded
    bool cardDirty_ = true;
    int travel_ = 0;                // slide distance in pixels; the window is this much wider
    double restX_ = 0.0;            // card position inside the window when fully shown
    double offsetX_ = 0.0;          // current card position inside the window
    double effectiveScale_ = 1.0;
    int holdMs_ = 3500;
    int entryMs_ = 0;
    double entryFromOpacity_ = 0.0; // where the entry animation starts, so a
    double entryFromSlide_ = 1.0;   // replacement continues from what is on screen
    double opacity_ = 0.0;          // last values applied by tick()
    double slide_ = 0.0;
    bool animating_ = false;
};

QString noticeTitle(Kind kind);
SoundEvent soundEvent(Kind kind);
bool playNoticeSound(SoundEvent event, const Settings &settings);
void stopNoticeSound();
QRect anchoredRect(const QRect &available, const QSize &size, Corner corner, int margin);
}
