#include "settings-dialog.hpp"
#include "toast.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QTimer>
#include <QToolButton>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

namespace obs_notifier {
SettingsDialog::SettingsDialog(QWidget *parent, const Settings &s,
                               std::function<bool(const Settings &)> apply,
                               std::function<void(const Settings &, Kind)> preview)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("OBS Notifier Settings"));
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumWidth(440);
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    enabled_ = new QCheckBox(QStringLiteral("Show notifications"), this);
    enabled_->setToolTip(QStringLiteral("Show a popup when the replay buffer, streaming, recording, or virtual camera starts or stops, and when a clip is saved."));
    enabled_->setChecked(s.enabled);
    auto heading = enabled_->font();
    heading.setBold(true);
    enabled_->setFont(heading);
    layout->addWidget(enabled_);

    const auto newSection = [this, layout](const QString &title) {
        auto *box = new QGroupBox(title, this);
        auto *form = new QFormLayout(box);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(8);
        layout->addWidget(box);
        return std::pair{box, form};
    };

    // Behavior: where, how long, and what plays.
    auto [behaviorBox, behavior] = newSection(QStringLiteral("Behavior"));
    display_ = new QComboBox(behaviorBox);
    display_->addItem(QStringLiteral("Screen with the active window"), QStringLiteral("active"));
    display_->addItem(QStringLiteral("Screen with OBS"), QStringLiteral("obs"));
    for (auto *screen : QGuiApplication::screens()) {
        const auto size = screen->geometry().size();
        display_->addItem(QStringLiteral("%1 (%2 × %3)").arg(screen->name()).arg(size.width()).arg(size.height()), screen->name());
    }
    int displayIndex = display_->findData(s.display);
    if (displayIndex < 0) {
        display_->addItem(QStringLiteral("%1 (disconnected, uses main screen)").arg(s.display), s.display);
        displayIndex = display_->count() - 1;
    }
    display_->setCurrentIndex(displayIndex);
    behavior->addRow(QStringLiteral("Screen"), display_);
    corner_ = new QComboBox(behaviorBox);
    corner_->addItems({QStringLiteral("Top right"), QStringLiteral("Top left"),
                       QStringLiteral("Bottom right"), QStringLiteral("Bottom left")});
    corner_->setCurrentIndex(static_cast<int>(s.corner));
    behavior->addRow(QStringLiteral("Corner"), corner_);
    seconds_ = new QDoubleSpinBox(behaviorBox);
    seconds_->setRange(1, 15);
    seconds_->setSingleStep(0.5);
    seconds_->setDecimals(1);
    seconds_->setSuffix(QStringLiteral(" s"));
    seconds_->setValue(s.seconds);
    behavior->addRow(QStringLiteral("Duration"), seconds_);
    sound_ = new QCheckBox(QStringLiteral("Play sounds"), behaviorBox);
    sound_->setChecked(s.sound);
    behavior->addRow(QString(), sound_);

    // One row per sound: on/off switch, the file it plays, test, and reset.
    auto *soundGrid = new QGridLayout;
    soundGrid->setContentsMargins(20, 0, 0, 0);
    soundGrid->setHorizontalSpacing(6);
    soundGrid->setColumnStretch(1, 1);
    const struct {
        SoundEvent event;
        const char *label;
        const char *builtIn;
        bool enabled;
        QString path;
    } sounds[] = {
        {SoundEvent::Start, "Starting", "Built-in: soft swoosh up", s.soundStart, s.soundStartFile},
        {SoundEvent::Stop, "Stopping", "Built-in: soft swoosh down", s.soundStop, s.soundStopFile},
        {SoundEvent::Saved, "Clip saved", "Built-in: soft tap", s.soundSaved, s.soundSavedFile},
    };
    QList<QWidget *> soundWidgets;
    for (int i = 0; i < 3; ++i) {
        auto &row = soundRows_[i];
        const QString builtIn = QString::fromUtf8(sounds[i].builtIn);
        row.enabled = new QCheckBox(QString::fromUtf8(sounds[i].label), behaviorBox);
        row.enabled->setChecked(sounds[i].enabled);
        row.file = new QPushButton(behaviorBox);
        row.file->setStyleSheet(QStringLiteral("text-align: left; padding-left: 8px;"));
        row.path = sounds[i].path;
        const auto showFile = [&row, builtIn] {
            row.file->setText(row.path.isEmpty() ? builtIn : QFileInfo(row.path).fileName());
            row.file->setToolTip(row.path.isEmpty() ? QStringLiteral("Click to choose your own .wav file.") : row.path);
        };
        showFile();
        auto *test = new QToolButton(behaviorBox);
        test->setText(QStringLiteral("▶"));
        test->setToolTip(QStringLiteral("Play this sound"));
        auto *reset = new QToolButton(behaviorBox);
        reset->setText(QStringLiteral("Reset"));
        reset->setToolTip(QStringLiteral("Use the built-in sound"));
        const SoundEvent event = sounds[i].event;
        connect(row.file, &QPushButton::clicked, this, [this, &row, showFile] {
            const QString start = row.path.isEmpty() ? QString() : QFileInfo(row.path).absolutePath();
            const QString chosen = QFileDialog::getOpenFileName(this, QStringLiteral("Choose a sound"), start,
                                                                QStringLiteral("WAV audio (*.wav)"));
            if (!chosen.isEmpty()) {
                row.path = chosen;
                showFile();
            }
        });
        connect(reset, &QToolButton::clicked, this, [&row, showFile] {
            row.path.clear();
            showFile();
        });
        connect(test, &QToolButton::clicked, this, [this, event] { playNoticeSound(event, values()); });
        soundGrid->addWidget(row.enabled, i, 0);
        soundGrid->addWidget(row.file, i, 1);
        soundGrid->addWidget(test, i, 2);
        soundGrid->addWidget(reset, i, 3);
        soundWidgets << row.enabled << row.file << test << reset;
    }
    behavior->addRow(soundGrid);
    const auto updateSoundRows = [this, soundWidgets] {
        for (auto *widget : soundWidgets)
            widget->setEnabled(sound_->isChecked());
    };
    connect(sound_, &QCheckBox::toggled, this, updateSoundRows);
    updateSoundRows();
    filename_ = new QCheckBox(QStringLiteral("Show the filename of saved clips and recordings"), behaviorBox);
    filename_->setChecked(s.showFilename);
    behavior->addRow(QString(), filename_);

    // Look: size, spacing, and text.
    auto [lookBox, look] = newSection(QStringLiteral("Size and text"));
    scale_ = new QComboBox(lookBox);
    for (int percent : {75, 100, 125, 150, 175, 200})
        scale_->addItem(percent == 100 ? QStringLiteral("100% (automatic for your screen)")
                                       : QStringLiteral("%1%").arg(percent), percent);
    if (scale_->findData(s.scale) < 0)
        scale_->addItem(QStringLiteral("%1%").arg(s.scale), s.scale);
    scale_->setCurrentIndex(scale_->findData(s.scale));
    scale_->setToolTip(QStringLiteral("Notifications already scale with each screen's resolution.\nThis makes them larger or smaller on top of that."));
    look->addRow(QStringLiteral("Size"), scale_);
    margin_ = new QSpinBox(lookBox);
    margin_->setRange(0, 160);
    margin_->setSuffix(QStringLiteral(" px"));
    margin_->setValue(s.margin);
    margin_->setToolTip(QStringLiteral("Distance from the screen edge on a 1080p screen. It scales with the resolution."));
    look->addRow(QStringLiteral("Edge spacing"), margin_);
    fontFamily_ = new QComboBox(lookBox);
    fontFamily_->addItem(QStringLiteral("Default (Segoe UI)"), QString());
    for (const auto &family : QFontDatabase::families())
        if (!QFontDatabase::isPrivateFamily(family))
            fontFamily_->addItem(family, family);
    int familyIndex = fontFamily_->findData(s.fontFamily);
    if (familyIndex < 0) {
        fontFamily_->addItem(QStringLiteral("%1 (not installed)").arg(s.fontFamily), s.fontFamily);
        familyIndex = fontFamily_->count() - 1;
    }
    fontFamily_->setCurrentIndex(familyIndex);
    fontFamily_->setMaxVisibleItems(16);
    look->addRow(QStringLiteral("Font"), fontFamily_);
    auto *textRow = new QHBoxLayout;
    fontSize_ = new QSpinBox(lookBox);
    fontSize_->setRange(10, 28);
    fontSize_->setSuffix(QStringLiteral(" px"));
    fontSize_->setValue(s.fontSize);
    fontSize_->setToolTip(QStringLiteral("Title size on a 1080p screen. The small label and filename follow it."));
    titleWeight_ = new QComboBox(lookBox);
    titleWeight_->addItem(QStringLiteral("Regular"), 400);
    titleWeight_->addItem(QStringLiteral("Medium"), 500);
    titleWeight_->addItem(QStringLiteral("Semibold"), 600);
    titleWeight_->addItem(QStringLiteral("Bold"), 700);
    titleWeight_->setCurrentIndex(std::max(0, titleWeight_->findData(s.titleWeight)));
    titleWeight_->setToolTip(QStringLiteral("Weight of the title text"));
    textRow->addWidget(fontSize_, 1);
    textRow->addWidget(titleWeight_, 2);
    look->addRow(QStringLiteral("Text size"), textRow);

    // Colors: follow the theme, or pick three colors.
    auto [colorBox, colors] = newSection(QStringLiteral("Colors"));
    colorMode_ = new QComboBox(colorBox);
    colorMode_->addItem(QStringLiteral("Match the OBS theme"));
    colorMode_->addItem(QStringLiteral("Custom"));
    colorMode_->setCurrentIndex(s.customColors ? 1 : 0);
    colors->addRow(QStringLiteral("Style"), colorMode_);
    if (s.customColors) {
        background_ = s.backgroundColor;
        text_ = s.textColor;
        accent_ = s.accentColor;
    } else {
        // Start custom colors from the current theme so switching is a gentle change.
        const auto palette = parent ? parent->palette() : this->palette();
        background_ = palette.color(QPalette::Active, QPalette::Window);
        text_ = palette.color(QPalette::Active, QPalette::WindowText);
        accent_ = palette.color(QPalette::Active, QPalette::Highlight);
    }
    const auto showColor = [](QPushButton *button, const QColor &color) {
        QPixmap swatch(18, 18);
        swatch.fill(Qt::transparent);
        QPainter painter(&swatch);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(128, 128, 128), 1));
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(0.5, 0.5, 17, 17), 4, 4);
        painter.end();
        button->setIcon(QIcon(swatch));
        button->setIconSize(swatch.size());
        button->setText(color.alpha() < 255 ? color.name(QColor::HexArgb).toUpper()
                                            : color.name(QColor::HexRgb).toUpper());
    };
    const auto addColor = [this, colorBox, colors, showColor](const QString &label, QColor *color, bool alpha) {
        auto *button = new QPushButton(colorBox);
        button->setToolTip(QStringLiteral("Pick a color, or type a hex (#RRGGBB) or RGB value in the picker."));
        button->setStyleSheet(QStringLiteral("text-align: left; padding-left: 8px;"));
        showColor(button, *color);
        connect(button, &QPushButton::clicked, this, [this, button, color, alpha, label, showColor] {
            QColorDialog::ColorDialogOptions options = QColorDialog::DontUseNativeDialog;
            if (alpha)
                options |= QColorDialog::ShowAlphaChannel;
            const QColor picked = QColorDialog::getColor(*color, this, label, options);
            if (picked.isValid()) {
                *color = picked;
                showColor(button, picked);
            }
        });
        colors->addRow(label, button);
        return button;
    };
    backgroundButton_ = addColor(QStringLiteral("Background"), &background_, true);
    textButton_ = addColor(QStringLiteral("Text"), &text_, false);
    accentButton_ = addColor(QStringLiteral("Accent"), &accent_, false);
    const auto updateColorRows = [this, colors] {
        const bool custom = colorMode_->currentIndex() == 1;
        for (auto *button : {backgroundButton_, textButton_, accentButton_})
            colors->setRowVisible(button, custom);
        // Let the layout drop the hidden rows before shrinking to fit.
        QTimer::singleShot(0, this, [this] { resize(width(), sizeHint().height()); });
    };
    connect(colorMode_, &QComboBox::currentIndexChanged, this, updateColorRows);

    // Everything below the main switch is only relevant while notifications are on.
    const auto updateEnabled = [this, behaviorBox, lookBox, colorBox] {
        for (auto *box : {behaviorBox, lookBox, colorBox})
            box->setEnabled(enabled_->isChecked());
    };
    connect(enabled_, &QCheckBox::toggled, this, updateEnabled);
    updateEnabled();

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
    previewButton->setToolTip(QStringLiteral("Show this notification with the settings above, without saving."));
    previewRow->addWidget(new QLabel(QStringLiteral("Try it:"), this));
    previewRow->addWidget(previewKind_, 1);
    previewRow->addWidget(previewButton);
    layout->addLayout(previewRow);
    connect(previewButton, &QPushButton::clicked, this, [this, preview] {
        preview(values(), static_cast<Kind>(previewKind_->currentData().toInt()));
    });

    auto *note = new QLabel(QStringLiteral("Notifications appear over the desktop and borderless games. Display Capture records them too; use Game Capture to keep them out of your clips."), this);
    note->setWordWrap(true);
    auto noteFont = note->font();
    noteFont.setPointSizeF(noteFont.pointSizeF() * 0.9);
    note->setFont(noteFont);
    note->setForegroundRole(QPalette::PlaceholderText);
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
    updateColorRows();
}

Settings SettingsDialog::values() const
{
    Settings s;
    s.enabled = enabled_->isChecked();
    s.showFilename = filename_->isChecked();
    s.sound = sound_->isChecked();
    s.soundStart = soundRows_[0].enabled->isChecked();
    s.soundStop = soundRows_[1].enabled->isChecked();
    s.soundSaved = soundRows_[2].enabled->isChecked();
    s.soundStartFile = soundRows_[0].path;
    s.soundStopFile = soundRows_[1].path;
    s.soundSavedFile = soundRows_[2].path;
    s.display = display_->currentData().toString();
    s.corner = static_cast<Corner>(corner_->currentIndex());
    s.seconds = seconds_->value();
    s.scale = scale_->currentData().toInt();
    s.margin = margin_->value();
    s.fontFamily = fontFamily_->currentData().toString();
    s.fontSize = fontSize_->value();
    s.titleWeight = titleWeight_->currentData().toInt();
    s.customColors = colorMode_->currentIndex() == 1;
    s.backgroundColor = background_;
    s.textColor = text_;
    s.accentColor = accent_;
    return s;
}
}
