#pragma once

#include "settings.hpp"
#include <obs.h>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <array>
#include <functional>
#include <vector>

namespace obs_notifier {

class ShortcutMonitor final : public QObject {
public:
    ShortcutMonitor(QWidget *obsWindow, std::function<void(Kind)> notify);
    ~ShortcutMonitor() override;
    void setEnabled(bool enabled);
    void stop();

private:
    struct Binding {
        obs_key_combination_t combo;
        int virtualKey;
    };
    static void bindingsChanged(void *data, calldata_t *);
    static bool collectBinding(void *data, size_t, obs_hotkey_binding_t *binding);
    void poll();
    void refreshBindings();
    std::array<bool, 8> pressedActions() const;
    bool shortcutsAllowed() const;
    QPointer<QWidget> obsWindow_;
    std::function<void(Kind)> notify_;
    QTimer timer_;
    signal_handler_t *signals_ = nullptr;
    std::array<std::vector<Binding>, 8> bindings_;
    std::array<bool, 8> wasPressed_{};
    std::array<bool, 4> wasActive_{};
    bool dirty_ = true;
};
}
