#include "toast.hpp"

#include <QApplication>
#include <QEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWindow>
#include <algorithm>
#include <cmath>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

namespace obs_notifier {
namespace {
constexpr int CardWidth = 460;
constexpr int CardHeight = 104;
constexpr int EnterMs = 220;
constexpr int LeaveMs = 180;
constexpr int ChimeResourceId = 101;
constexpr int ChimeDurationMs = 720;

double smoothStep(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t * (t * (6.0 * t - 15.0) + 10.0);
}

Kind iconKind(Kind kind)
{
    switch (kind) {
    case Kind::ReplayAlreadyRunning: return Kind::ReplayStarted;
    case Kind::ReplayAlreadyStopped: return Kind::ReplayStopped;
    case Kind::StreamingAlreadyRunning: return Kind::StreamingStarted;
    case Kind::StreamingAlreadyStopped: return Kind::StreamingStopped;
    case Kind::RecordingAlreadyRunning: return Kind::RecordingStarted;
    case Kind::RecordingAlreadyStopped: return Kind::RecordingStopped;
    case Kind::VirtualCameraAlreadyRunning: return Kind::VirtualCameraStarted;
    case Kind::VirtualCameraAlreadyStopped: return Kind::VirtualCameraStopped;
    default: return kind;
    }
}

bool leftCorner(Corner corner)
{
    return corner == Corner::TopLeft || corner == Corner::BottomLeft;
}

QColor blend(const QColor &a, const QColor &b, double amount)
{
    return QColor::fromRgbF(a.redF() * (1 - amount) + b.redF() * amount,
                           a.greenF() * (1 - amount) + b.greenF() * amount,
                           a.blueF() * (1 - amount) + b.blueF() * amount);
}

QString noticeSubtitle(Kind kind, const QString &filename, bool showFilename)
{
    switch (kind) {
    case Kind::ReplayStarted: return QStringLiteral("Ready to capture your next moment");
    case Kind::ReplayStopped: return QStringLiteral("Replay capture has stopped");
    case Kind::ReplaySaved:
        return showFilename && !filename.isEmpty() ? QFileInfo(filename).fileName()
                                                   : QStringLiteral("Your replay is ready");
    case Kind::StreamingStarted: return QStringLiteral("You are live");
    case Kind::StreamingStopped: return QStringLiteral("Your live stream has ended");
    case Kind::RecordingStarted: return QStringLiteral("Your recording is running");
    case Kind::RecordingStopped: return QStringLiteral("Your recording has finished");
    case Kind::VirtualCameraStarted: return QStringLiteral("Ready for your camera apps");
    case Kind::VirtualCameraStopped: return QStringLiteral("Virtual camera output has stopped");
    case Kind::ReplayAlreadyRunning: return QStringLiteral("Replay capture is still running");
    case Kind::ReplayAlreadyStopped: return QStringLiteral("Replay capture is already stopped");
    case Kind::StreamingAlreadyRunning: return QStringLiteral("Your stream is still live");
    case Kind::StreamingAlreadyStopped: return QStringLiteral("There is no active stream to stop");
    case Kind::RecordingAlreadyRunning: return QStringLiteral("Your recording is still running");
    case Kind::RecordingAlreadyStopped: return QStringLiteral("There is no active recording to stop");
    case Kind::VirtualCameraAlreadyRunning: return QStringLiteral("Virtual camera output is still running");
    case Kind::VirtualCameraAlreadyStopped: return QStringLiteral("Virtual camera output is already stopped");
    }
    return {};
}
}

QString noticeTitle(Kind kind)
{
    switch (kind) {
    case Kind::ReplayStarted: return QStringLiteral("Replay buffer is on");
    case Kind::ReplayStopped: return QStringLiteral("Replay buffer is off");
    case Kind::ReplaySaved: return QStringLiteral("Clip saved");
    case Kind::StreamingStarted: return QStringLiteral("Streaming started");
    case Kind::StreamingStopped: return QStringLiteral("Streaming stopped");
    case Kind::RecordingStarted: return QStringLiteral("Recording started");
    case Kind::RecordingStopped: return QStringLiteral("Recording stopped");
    case Kind::VirtualCameraStarted: return QStringLiteral("Virtual camera started");
    case Kind::VirtualCameraStopped: return QStringLiteral("Virtual camera stopped");
    case Kind::ReplayAlreadyRunning: return QStringLiteral("Replay buffer already on");
    case Kind::ReplayAlreadyStopped: return QStringLiteral("Replay buffer already off");
    case Kind::StreamingAlreadyRunning: return QStringLiteral("Streaming already live");
    case Kind::StreamingAlreadyStopped: return QStringLiteral("Streaming already stopped");
    case Kind::RecordingAlreadyRunning: return QStringLiteral("Recording already running");
    case Kind::RecordingAlreadyStopped: return QStringLiteral("Recording already stopped");
    case Kind::VirtualCameraAlreadyRunning: return QStringLiteral("Virtual camera already on");
    case Kind::VirtualCameraAlreadyStopped: return QStringLiteral("Virtual camera already off");
    }
    return {};
}

QRect anchoredRect(const QRect &available, const QSize &size, Corner corner, int margin)
{
    const int xMargin = std::clamp(margin, 0, std::max(0, (available.width() - size.width()) / 2));
    const int yMargin = std::clamp(margin, 0, std::max(0, (available.height() - size.height()) / 2));
    const bool top = corner == Corner::TopRight || corner == Corner::TopLeft;
    const int x = leftCorner(corner) ? available.left() + xMargin
                                    : available.right() - size.width() + 1 - xMargin;
    const int y = top ? available.top() + yMargin
                      : available.bottom() - size.height() + 1 - yMargin;
    return {QPoint(x, y), size};
}

Toast::Toast(QWidget *obsWindow)
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                           Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput),
      obsWindow_(obsWindow)
{
    setObjectName(QStringLiteral("obsNotifierToast"));
    setWindowTitle(QStringLiteral("OBS Notifier"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    holdTimer_.setSingleShot(true);
    holdTimer_.setTimerType(Qt::PreciseTimer);
    connect(&holdTimer_, &QTimer::timeout, this, [this] { tick(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this] { if (isVisible()) place(); });
    refreshTheme();
}

Toast::~Toast()
{
    animating_ = false;
    holdTimer_.stop();

    if (chimeElapsed_.isValid() && chimeElapsed_.elapsed() < ChimeDurationMs)
        PlaySoundW(nullptr, nullptr, 0);
}

void Toast::setSettings(const Settings &settings)
{
    settings_ = settings;
    dismissAll();
}

void Toast::refreshTheme()
{
    colors_ = obsWindow_ ? obsWindow_->palette() : qApp->palette();
    update();
}

void Toast::dismissAll()
{
    animating_ = false;
    holdTimer_.stop();
    hide();
}

QScreen *Toast::targetScreen() const
{
    const auto screens = QGuiApplication::screens();
    if (settings_.display == QStringLiteral("active")) {

        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        const auto monitor = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
        if (GetMonitorInfoW(monitor, &info)) {
            const auto name = QString::fromWCharArray(info.szDevice);
            for (auto *screen : screens)
                if (screen->name() == name)
                    return screen;
        }
    } else if (settings_.display == QStringLiteral("obs")) {
        if (obsWindow_ && obsWindow_->screen())
            return obsWindow_->screen();
    } else {
        for (auto *screen : screens)
            if (screen->name() == settings_.display)
                return screen;
    }
    return QGuiApplication::primaryScreen();
}

void Toast::place()
{
    auto *screen = targetScreen();
    if (!screen)
        return;
    const auto area = screen->availableGeometry();
    effectiveScale_ = std::min({settings_.scale / 100.0,
                                area.width() / double(CardWidth + 16),
                                area.height() / double(CardHeight + 16)});
    const QSize cardSize(qRound(CardWidth * effectiveScale_), qRound(CardHeight * effectiveScale_));

    resize(cardSize + QSize(1, 0));
    if (windowHandle())
        windowHandle()->setScreen(screen);
    anchor_ = anchoredRect(area, cardSize, settings_.corner, settings_.margin).topLeft();
    move(anchor_);
}

void Toast::showNotice(Notice notice, bool preview)
{
    if (!preview && !settings_.accepts(notice.kind))
        return;

    animateEntry_ = !isVisible();
    animating_ = false;
    holdTimer_.stop();
    current_ = std::move(notice);
    refreshTheme();
    place();
    holdMs_ = qRound(settings_.seconds * 1000);
    setWindowOpacity(animateEntry_ ? 0.0 : 1.0);

    const auto handle = reinterpret_cast<HWND>(winId());
    windowHandle()->installEventFilter(this);
    SetWindowLongPtrW(handle, GWL_EXSTYLE, GetWindowLongPtrW(handle, GWL_EXSTYLE) |
                      WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
    show();
    SetWindowPos(handle, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    elapsed_.start();
    if (settings_.sound) {
        HMODULE module = nullptr;

        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(&ChimeResourceId), &module) &&
            PlaySoundW(MAKEINTRESOURCEW(ChimeResourceId), module,
                       SND_RESOURCE | SND_ASYNC | SND_NODEFAULT))
            chimeElapsed_.start();
    }
    tick();
    update();
}

void Toast::requestFrame()
{
    if (animating_ && isVisible() && windowHandle())

        windowHandle()->requestUpdate();
}

bool Toast::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == windowHandle() && event->type() == QEvent::UpdateRequest && animating_)
        tick();

    return QWidget::eventFilter(watched, event);
}

void Toast::tick()
{
    const double ms = elapsed_.nsecsElapsed() / 1000000.0;
    const int entryMs = animateEntry_ ? EnterMs : 0;
    double opacity = 1.0;
    double slide = 0.0;
    if (ms < entryMs) {
        const double t = ms / double(entryMs);
        const double eased = smoothStep(t);
        opacity = eased;
        slide = 1.0 - eased;
    } else if (ms > entryMs + holdMs_) {
        const double t = std::min(1.0, (ms - entryMs - holdMs_) / double(LeaveMs));
        slide = smoothStep(t);
        opacity = 1.0 - slide;
    }
    if (ms >= entryMs + holdMs_ + LeaveMs) {
        animating_ = false;
        holdTimer_.stop();
        hide();
        return;
    }
    setWindowOpacity(opacity);
    const int direction = leftCorner(settings_.corner) ? -1 : 1;
    const double x = direction * 32 * effectiveScale_ * slide;
    const int wholeX = static_cast<int>(std::floor(x));
    fractionalX_ = x - wholeX;
    move(anchor_ + QPoint(wholeX, 0));
    if (ms >= entryMs && ms < entryMs + holdMs_) {

        animating_ = false;
        holdTimer_.start(std::max(1, static_cast<int>(std::ceil(entryMs + holdMs_ - ms))));
    } else {
        animating_ = true;
        requestFrame();
    }
}

void Toast::drawCard(QPainter &p) const
{
    p.setRenderHint(QPainter::Antialiasing);
    const auto background = colors_.color(QPalette::Active, QPalette::Window);
    const auto foreground = colors_.color(QPalette::Active, QPalette::WindowText);
    auto accent = colors_.color(QPalette::Active, QPalette::Highlight);

    if (std::abs(accent.lightnessF() - background.lightnessF()) < 0.12)
        accent = blend(accent, foreground, 0.42);
    const auto muted = blend(foreground, background, 0.34);
    p.setPen(QPen(blend(background, foreground, 0.13), 1));
    p.setBrush(background);
    p.drawRoundedRect(QRectF(0.5, 0.5, CardWidth - 1, CardHeight - 1), 5, 5);
    p.setClipRect(QRectF(0, 0, CardWidth, CardHeight));
    p.fillRect(QRectF(0, 0, 5, CardHeight), accent);

    p.save();
    p.translate(53, 51);
    p.setPen(QPen(accent, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const Kind icon = iconKind(current_.kind);
    if (icon == Kind::ReplaySaved) {
        p.drawRoundedRect(QRectF(-20, -17, 40, 34), 4, 4);
        QPainterPath check;
        check.moveTo(-9, 0); check.lineTo(-2, 7); check.lineTo(11, -7);
        p.drawPath(check);
    } else if (icon == Kind::StreamingStarted || icon == Kind::StreamingStopped) {

        for (const int radius : {14, 23}) {
            const QRectF arc(-radius, -radius, radius * 2, radius * 2);
            p.drawArc(arc, -55 * 16, 110 * 16);
            p.drawArc(arc, 125 * 16, 110 * 16);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawEllipse(QPointF(0, 0), 5, 5);
    } else if (icon == Kind::RecordingStarted || icon == Kind::RecordingStopped) {
        p.drawEllipse(QPointF(0, 0), 22, 22);
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        if (icon == Kind::RecordingStarted)
            p.drawEllipse(QPointF(0, 0), 9, 9);
        else
            p.drawRoundedRect(QRectF(-8, -8, 16, 16), 1.5, 1.5);
    } else if (icon == Kind::VirtualCameraStarted || icon == Kind::VirtualCameraStopped) {
        p.drawRoundedRect(QRectF(-23, -16, 31, 32), 4, 4);
        p.drawPolygon(QPolygonF{QPointF(8, -7), QPointF(23, -15), QPointF(23, 15), QPointF(8, 7)});
    } else {
        p.drawArc(QRectF(-21, -21, 42, 42), 35 * 16, 140 * 16);
        p.drawArc(QRectF(-21, -21, 42, 42), 215 * 16, 140 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawPolygon(QPolygonF{QPointF(-21, -2), QPointF(-26, -13), QPointF(-12, -10)});
        p.drawPolygon(QPolygonF{QPointF(21, 2), QPointF(26, 13), QPointF(12, 10)});
        if (icon == Kind::ReplayStarted)
            p.drawEllipse(QPointF(0, 0), 6, 6);
        else {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(accent, 3.5, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(-6, -6), QPointF(6, 6));
            p.drawLine(QPointF(-6, 6), QPointF(6, -6));
        }
    }
    if (icon == Kind::StreamingStopped || icon == Kind::VirtualCameraStopped) {

        p.setPen(QPen(background, 8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(-23, 23), QPointF(23, -23));
        p.setPen(QPen(accent, 3.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(-23, 23), QPointF(23, -23));
    }
    p.restore();

    QFont labelFont = qApp->font();
    labelFont.setPixelSize(10);
    labelFont.setWeight(QFont::DemiBold);
    labelFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    p.setFont(labelFont);
    p.setPen(muted);
    p.drawText(QRectF(98, 15, 340, 17), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("OBS"));

    QFont titleFont = qApp->font();
    titleFont.setPixelSize(23);
    titleFont.setWeight(QFont::Medium);
    p.setFont(titleFont);
    p.setPen(foreground);
    p.drawText(QRectF(97, 34, 343, 32), Qt::AlignVCenter | Qt::AlignLeft,
               noticeTitle(current_.kind));

    const auto subtitle = noticeSubtitle(current_.kind, current_.filename, settings_.showFilename);
    QFont detailFont = qApp->font();
    detailFont.setPixelSize(12);
    p.setFont(detailFont);
    p.setPen(muted);
    p.drawText(QRectF(98, 69, 340, 19), Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(detailFont).elidedText(subtitle, Qt::ElideMiddle, 340));
}

void Toast::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.translate(fractionalX_, 0.0);
    p.scale(effectiveScale_, effectiveScale_);
    drawCard(p);
}

QImage Toast::snapshot() const
{
    QImage image(CardWidth * 2, CardHeight * 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.scale(2, 2);
    drawCard(painter);
    return image;
}
}
