#include "settings-dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QVBoxLayout>

namespace obs_notifier {
SettingsDialog::SettingsDialog(QWidget *parent, const Settings &s,
                               std::function<bool(const Settings &)> apply,
                               std::function<void(const Settings &, Kind)> preview)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("OBS Notifier"));
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumWidth(480);
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(16);
    auto *heading = new QLabel(QStringLiteral("OBS Notifier"), this);
    auto font = heading->font();
    font.setPointSize(font.pointSize() + 4);
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
    auto *description = new QLabel(QStringLiteral("Replay buffer, clips, streaming, recording, and virtual camera.\nColors follow your current OBS theme automatically."), this);
    description->setWordWrap(true);
    layout->addWidget(description);
    enabled_ = new QCheckBox(QStringLiteral("Enable all notifications"), this);
    enabled_->setChecked(s.enabled);
    layout->addWidget(enabled_);

    auto *appearance = new QGroupBox(QStringLiteral("Appearance"), this);
    auto *form = new QFormLayout(appearance);
    display_ = new QComboBox(appearance);
    display_->addItem(QStringLiteral("Display with the active window"), QStringLiteral("active"));
    display_->addItem(QStringLiteral("Display with OBS"), QStringLiteral("obs"));
    for (auto *screen : QGuiApplication::screens()) {
        const auto size = screen->geometry().size();
        display_->addItem(QStringLiteral("%1 (%2 × %3)").arg(screen->name()).arg(size.width()).arg(size.height()), screen->name());
    }
    int displayIndex = display_->findData(s.display);
    if (displayIndex < 0) {
        display_->addItem(QStringLiteral("%1 (disconnected; uses primary)").arg(s.display), s.display);
        displayIndex = display_->count() - 1;
    }
    display_->setCurrentIndex(displayIndex);
    form->addRow(QStringLiteral("Display"), display_);
    corner_ = new QComboBox(appearance);
    corner_->addItems({QStringLiteral("Top right"), QStringLiteral("Top left"),
                       QStringLiteral("Bottom right"), QStringLiteral("Bottom left")});
    corner_->setCurrentIndex(static_cast<int>(s.corner));
    form->addRow(QStringLiteral("Position"), corner_);
    seconds_ = new QDoubleSpinBox(appearance);
    seconds_->setRange(1, 15);
    seconds_->setSingleStep(0.5);
    seconds_->setDecimals(1);
    seconds_->setSuffix(QStringLiteral(" seconds"));
    seconds_->setValue(s.seconds);
    form->addRow(QStringLiteral("Show for"), seconds_);
    scale_ = new QComboBox(appearance);
    for (int percent : {75, 100, 125, 150, 175, 200})
        scale_->addItem(QStringLiteral("%1% %2").arg(percent).arg(percent == 100 ? QStringLiteral("(default)") : QString()), percent);
    if (scale_->findData(s.scale) < 0)
        scale_->addItem(QStringLiteral("%1%").arg(s.scale), s.scale);
    scale_->setCurrentIndex(scale_->findData(s.scale));
    form->addRow(QStringLiteral("Size"), scale_);
    margin_ = new QSpinBox(appearance);
    margin_->setRange(0, 160);
    margin_->setSuffix(QStringLiteral(" px"));
    margin_->setValue(s.margin);
    form->addRow(QStringLiteral("Edge spacing"), margin_);
    filename_ = new QCheckBox(QStringLiteral("Show the saved clip's filename"), appearance);
    filename_->setChecked(s.showFilename);
    form->addRow(filename_);
    sound_ = new QCheckBox(QStringLiteral("Play a notification sound"), appearance);
    sound_->setChecked(s.sound);
    form->addRow(sound_);
    layout->addWidget(appearance);

    auto *previewRow = new QHBoxLayout;
    previewKind_ = new QComboBox(this);
    const auto addPreview = [this](const QString &label, Kind kind) {
        previewKind_->addItem(label, static_cast<int>(kind));
    };
    addPreview(QStringLiteral("Streaming started"), Kind::StreamingStarted);
    addPreview(QStringLiteral("Streaming stopped"), Kind::StreamingStopped);
    addPreview(QStringLiteral("Streaming already live"), Kind::StreamingAlreadyRunning);
    addPreview(QStringLiteral("Streaming already stopped"), Kind::StreamingAlreadyStopped);
    addPreview(QStringLiteral("Recording started"), Kind::RecordingStarted);
    addPreview(QStringLiteral("Recording stopped"), Kind::RecordingStopped);
    addPreview(QStringLiteral("Recording already running"), Kind::RecordingAlreadyRunning);
    addPreview(QStringLiteral("Recording already stopped"), Kind::RecordingAlreadyStopped);
    addPreview(QStringLiteral("Replay buffer on"), Kind::ReplayStarted);
    addPreview(QStringLiteral("Replay buffer off"), Kind::ReplayStopped);
    addPreview(QStringLiteral("Clip saved"), Kind::ReplaySaved);
    addPreview(QStringLiteral("Replay buffer already on"), Kind::ReplayAlreadyRunning);
    addPreview(QStringLiteral("Replay buffer already off"), Kind::ReplayAlreadyStopped);
    addPreview(QStringLiteral("Virtual camera started"), Kind::VirtualCameraStarted);
    addPreview(QStringLiteral("Virtual camera stopped"), Kind::VirtualCameraStopped);
    addPreview(QStringLiteral("Virtual camera already on"), Kind::VirtualCameraAlreadyRunning);
    addPreview(QStringLiteral("Virtual camera already off"), Kind::VirtualCameraAlreadyStopped);
    auto *previewButton = new QPushButton(QStringLiteral("Preview"), this);
    previewRow->addWidget(previewKind_, 1);
    previewRow->addWidget(previewButton);
    layout->addLayout(previewRow);
    connect(previewButton, &QPushButton::clicked, this, [this, preview] {
        preview(values(), static_cast<Kind>(previewKind_->currentData().toInt()));
    });
    auto *note = new QLabel(QStringLiteral("Works over desktop and borderless games. Display Capture can include the popup.\nUse Game Capture to keep desktop notifications out of your clips."), this);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this, apply] {
        if (apply(values()))
            accept();
        else
            QMessageBox::warning(this, QStringLiteral("OBS Notifier"),
                                  QStringLiteral("The settings could not be saved. Check that the OBS configuration folder is writable."));
    });
}

Settings SettingsDialog::values() const
{
    Settings s;
    s.enabled = enabled_->isChecked();
    s.showFilename = filename_->isChecked();
    s.sound = sound_->isChecked();
    s.display = display_->currentData().toString();
    s.corner = static_cast<Corner>(corner_->currentIndex());
    s.seconds = seconds_->value();
    s.scale = scale_->currentData().toInt();
    s.margin = margin_->value();
    return s;
}
}
