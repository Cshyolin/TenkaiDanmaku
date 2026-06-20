#pragma once

#include <QDialog>

class QFontComboBox;
class QSpinBox;
class QSlider;
class QComboBox;
class QLineEdit;
class QLabel;

/// Configuration dialog opened from the tray menu.
class ConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigDialog(QWidget *parent = nullptr);

    /// Load settings from QSettings into the UI controls.
    void loadSettings();

    /// Save UI control values to QSettings.
    void saveSettings();

signals:
    /// Emitted after settings are saved so other modules can apply them.
    void settingsChanged();

private:
    void setupUi();

    QFontComboBox *m_fontCombo     = nullptr;
    QSpinBox      *m_fontSize      = nullptr;
    QSpinBox      *m_httpPort      = nullptr;
    QSlider       *m_trackAreaSlider = nullptr;
    QLabel        *m_trackAreaLabel = nullptr;
    QComboBox     *m_speedCombo    = nullptr;
    QSpinBox      *m_rateLimit     = nullptr;
    QLineEdit     *m_logDirEdit    = nullptr;
};
