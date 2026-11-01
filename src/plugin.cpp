#include "settings-dialog.hpp"
#include "toast.hpp"
#include "shortcut-monitor.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <memory>
#include <optional>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("OBS Notifier contributors")

MODULE_EXPORT const char *obs_module_name(void)
{
    return "OBS Notifier";
}

MODULE_EXPORT const char *obs_module_description(void)
{
    return "Theme-aware desktop notifications for replays, streaming, recording, and virtual camera.";
}

namespace obs_notifier {
namespace {
std::optional<Kind> noticeKind(enum obs_frontend_event event)
{
    switch (event) {
    case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED: return Kind::ReplayStarted;
    case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED: return Kind::ReplayStopped;
    case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED: return Kind::ReplaySaved;
    case OBS_FRONTEND_EVENT_STREAMING_STARTED: return Kind::StreamingStarted;
    case OBS_FRONTEND_EVENT_STREAMING_STOPPED: return Kind::StreamingStopped;
    case OBS_FRONTEND_EVENT_RECORDING_STARTED: return Kind::RecordingStarted;
    case OBS_FRONTEND_EVENT_RECORDING_STOPPED: return Kind::RecordingStopped;
    case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED: return Kind::VirtualCameraStarted;
    case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED: return Kind::VirtualCameraStopped;
    default: return std::nullopt;
    }
}
}

class Controller final : public QObject {
public:
    explicit Controller(QWidget *window) : QObject(window), toast_(std::make_unique<Toast>(window))
    {
        char *path = obs_module_config_path("settings.json");
        if (path) {
            configPath_ = QString::fromUtf8(path);
            bfree(path);
        }
        settings_ = Settings::load(configPath_);
        if (!configPath_.isEmpty() && !QFileInfo::exists(configPath_)) {
            const QDir configDirectory(QFileInfo(configPath_).absolutePath());
            settings_ = Settings::load(configDirectory.filePath("../replay-notifier/settings.json"));
        }
        toast_->setSettings(settings_);
        action_ = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction("OBS Notifier..."));
        if (action_)
            connect(action_, &QAction::triggered, this, [this] { openSettings(); });
        obs_frontend_add_event_callback(eventCallback, this);
        shortcuts_ = std::make_unique<ShortcutMonitor>(window, [this](Kind kind) { notify(kind, {}); });
        shortcuts_->setEnabled(settings_.enabled);
    }

    ~Controller() override
    {

        if (!exiting_)
            obs_frontend_remove_event_callback(eventCallback, this);
        shutdown();
        delete action_.data();
    }

    static void eventCallback(enum obs_frontend_event event, void *data)
    {
        auto *self = static_cast<Controller *>(data);

        QString filename;
        if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED) {
            char *path = obs_frontend_get_last_replay();
            if (path) {
                filename = QString::fromUtf8(path);
                bfree(path);
            }
        }
        if (event == OBS_FRONTEND_EVENT_EXIT) {
            self->exiting_ = true;
            self->shutdown();
            return;
        }
        if (!noticeKind(event) && event != OBS_FRONTEND_EVENT_THEME_CHANGED)
            return;
        QMetaObject::invokeMethod(self, [self, event, filename] { self->handle(event, filename); }, Qt::QueuedConnection);
    }

    void shutdown()
    {
        shortcuts_.reset();
        if (dialog_)
            delete dialog_.data();
        toast_.reset();
    }

private:
    void handle(enum obs_frontend_event event, const QString &filename)
    {
        if (exiting_ || !toast_)
            return;
        if (event == OBS_FRONTEND_EVENT_THEME_CHANGED) {
            toast_->refreshTheme();
            return;
        }
        const auto kind = noticeKind(event);
        if (!kind)
            return;
        notify(*kind, filename);
    }

    void notify(Kind kind, const QString &filename)
    {
        if (exiting_ || !toast_) return;

        if (previewing_) {
            toast_->setSettings(settings_);
            previewing_ = false;
        }
        toast_->showNotice({kind, filename});
        if (settings_.accepts(kind))
            blog(LOG_INFO, "[obs-notifier] %s", noticeTitle(kind).toUtf8().constData());
    }

    void openSettings()
    {
        if (exiting_ || !toast_)
            return;
        if (dialog_) {
            dialog_->show();
            dialog_->raise();
            dialog_->activateWindow();
            return;
        }
        dialog_ = new SettingsDialog(static_cast<QWidget *>(parent()), settings_,
            [this](const Settings &s) {
                if (!s.save(configPath_))
                    return false;
                settings_ = s;
                shortcuts_->setEnabled(settings_.enabled);
                toast_->setSettings(settings_);
                previewing_ = false;
                return true;
            },
            [this](const Settings &s, Kind kind) {
                toast_->setSettings(s);
                previewing_ = true;
                toast_->showNotice({kind, QStringLiteral("Replay - great moment.mkv")}, true);
            });
        connect(dialog_, &QDialog::finished, this, [this] {
            if (toast_ && previewing_)
                toast_->setSettings(settings_);
            previewing_ = false;
        });
        dialog_->show();
    }

    Settings settings_;
    QString configPath_;
    std::unique_ptr<Toast> toast_;
    std::unique_ptr<ShortcutMonitor> shortcuts_;
    QPointer<SettingsDialog> dialog_;
    QPointer<QAction> action_;
    bool exiting_ = false;
    bool previewing_ = false;
};

QPointer<Controller> controller;
}

bool obs_module_load(void)
{
    if (!qApp || QThread::currentThread() != qApp->thread()) {
        blog(LOG_ERROR, "[obs-notifier] Expected OBS's Qt UI thread during module load");
        return false;
    }
    auto *window = static_cast<QWidget *>(obs_frontend_get_main_window());
    if (!window)
        return false;
    obs_notifier::controller = new obs_notifier::Controller(window);
    blog(LOG_INFO, "[obs-notifier] Loaded OBS Notifier %s", OBS_NOTIFIER_VERSION);
    return true;
}

void obs_module_unload(void)
{
    delete obs_notifier::controller.data();
}
