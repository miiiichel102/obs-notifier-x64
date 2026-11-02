#pragma once

#include "settings.hpp"
#include <QDialog>
#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;

namespace obs_notifier {
class SettingsDialog final : public QDialog {
public:
    SettingsDialog(QWidget *parent, const Settings &settings,
                   std::function<bool(const Settings &)> apply,
                   std::function<void(const Settings &, Kind)> preview);

private:
    Settings values() const;
    QCheckBox *enabled_, *filename_, *sound_;
    QComboBox *display_, *corner_, *scale_, *previewKind_;
    QDoubleSpinBox *seconds_;
    QSpinBox *margin_, *fontSize_;
    QComboBox *fontFamily_, *titleWeight_, *colorMode_;
    QPushButton *backgroundButton_, *textButton_, *accentButton_;
    QColor background_, text_, accent_;
    struct SoundRow {
        QCheckBox *enabled;
        QPushButton *file;
        QString path; // empty = built-in sound
    };
    SoundRow soundRows_[3];
};
}
