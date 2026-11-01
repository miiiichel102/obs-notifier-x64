#include "shortcut-monitor.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <QApplication>
#include <QDialog>
#include <QMetaObject>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace obs_notifier {
namespace {
constexpr const char *Actions[] = {
    "OBSBasic.StartReplayBuffer", "OBSBasic.StopReplayBuffer",
    "OBSBasic.StartStreaming", "OBSBasic.StopStreaming",
    "OBSBasic.StartRecording", "OBSBasic.StopRecording",
    "OBSBasic.StartVirtualCam", "OBSBasic.StopVirtualCam"
};
constexpr Kind Already[] = {
    Kind::ReplayAlreadyRunning, Kind::ReplayAlreadyStopped,
    Kind::StreamingAlreadyRunning, Kind::StreamingAlreadyStopped,
    Kind::RecordingAlreadyRunning, Kind::RecordingAlreadyStopped,
    Kind::VirtualCameraAlreadyRunning, Kind::VirtualCameraAlreadyStopped
};
std::array<bool, 4> activeOutputs()
{
    return {obs_frontend_replay_buffer_active(), obs_frontend_streaming_active(),
            obs_frontend_recording_active(), obs_frontend_virtualcam_active()};
}
bool down(int key)
{
    return key != 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
}
}

ShortcutMonitor::ShortcutMonitor(QWidget *obsWindow, std::function<void(Kind)> notify)
    : QObject(obsWindow), obsWindow_(obsWindow), notify_(std::move(notify))
{
    signals_ = obs_get_signal_handler();
    for (const auto *signal : {"hotkey_register", "hotkey_unregister", "hotkey_bindings_changed"})
        signal_handler_connect(signals_, signal, bindingsChanged, this);
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.setInterval(8);
    connect(&timer_, &QTimer::timeout, this, [this] { poll(); });
}

ShortcutMonitor::~ShortcutMonitor()
{
    stop();
}

void ShortcutMonitor::setEnabled(bool enabled)
{
    dirty_ = true;
    if (enabled) timer_.start();
    else timer_.stop();
}

void ShortcutMonitor::stop()
{
    timer_.stop();
    if (!signals_) return;
    for (const auto *signal : {"hotkey_register", "hotkey_unregister", "hotkey_bindings_changed"})
        signal_handler_disconnect(signals_, signal, bindingsChanged, this);
    signals_ = nullptr;
}

void ShortcutMonitor::bindingsChanged(void *data, calldata_t *)
{
    auto *self = static_cast<ShortcutMonitor *>(data);
    QMetaObject::invokeMethod(self, [self] { self->dirty_ = true; }, Qt::QueuedConnection);
}

bool ShortcutMonitor::collectBinding(void *data, size_t, obs_hotkey_binding_t *binding)
{
    auto *self = static_cast<ShortcutMonitor *>(data);
    const auto *hotkey = obs_hotkey_binding_get_hotkey(binding);
    if (obs_hotkey_get_registerer_type(hotkey) != OBS_HOTKEY_REGISTERER_FRONTEND) return true;
    const char *name = obs_hotkey_get_name(hotkey);
    for (size_t i = 0; i < self->bindings_.size(); ++i) {
        if (std::strcmp(name, Actions[i]) != 0) continue;
        const auto combo = obs_hotkey_binding_get_key_combination(binding);
        if (!obs_key_combination_is_empty(combo))
            self->bindings_[i].push_back({combo, obs_key_to_virtual_key(combo.key)});
        break;
    }
    return true;
}

void ShortcutMonitor::refreshBindings()
{
    for (auto &bindings : bindings_) bindings.clear();
    obs_enum_hotkey_bindings(collectBinding, this);

    wasPressed_ = pressedActions();
    wasActive_ = activeOutputs();
    dirty_ = false;
}

std::array<bool, 8> ShortcutMonitor::pressedActions() const
{
    uint32_t modifiers = 0;
    if (down(VK_SHIFT)) modifiers |= INTERACT_SHIFT_KEY;
    if (down(VK_CONTROL)) modifiers |= INTERACT_CONTROL_KEY;
    if (down(VK_MENU)) modifiers |= INTERACT_ALT_KEY;
    if (down(VK_LWIN) || down(VK_RWIN)) modifiers |= INTERACT_COMMAND_KEY;
    std::array<bool, 8> pressed{};
    for (size_t i = 0; i < bindings_.size(); ++i)
        for (const auto &binding : bindings_[i])
            if (binding.combo.modifiers == modifiers &&
                (binding.combo.key == OBS_KEY_NONE || down(binding.virtualKey))) {
                pressed[i] = true;
                break;
            }
    return pressed;
}

bool ShortcutMonitor::shortcutsAllowed() const
{

    for (auto *window : QApplication::topLevelWidgets())
        if (window->isVisible() && qobject_cast<QDialog *>(window)) return false;
    auto *config = obs_frontend_get_user_config();
    const char *focusType = config ? config_get_string(config, "General", "HotkeyFocusType") : nullptr;
    const bool focused = qApp->applicationState() == Qt::ApplicationActive;
    if (focusType && std::strcmp(focusType, "DisableHotkeysInFocus") == 0 && focused) return false;
    if (focusType && std::strcmp(focusType, "DisableHotkeysOutOfFocus") == 0 && !focused) return false;
    return true;
}

void ShortcutMonitor::poll()
{
    if (dirty_) { refreshBindings(); return; }
    const auto pressed = pressedActions();
    const auto active = activeOutputs();
    if (shortcutsAllowed()) {
        for (size_t output = 0; output < active.size(); ++output) {
            const size_t start = output * 2;
            const size_t stop = start + 1;

            if (pressed[start] && !wasPressed_[start] && !pressed[stop] &&
                wasActive_[output] && active[output])
                notify_(Already[start]);
            else if (pressed[stop] && !wasPressed_[stop] && !pressed[start] &&
                     !wasActive_[output] && !active[output])
                notify_(Already[stop]);
        }
    }
    wasPressed_ = pressed;
    wasActive_ = active;
}
}
