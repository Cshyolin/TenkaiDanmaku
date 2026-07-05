#pragma once

#include <QDialog>

class QFontComboBox;
class QSpinBox;
class QSlider;
class QComboBox;
class QLineEdit;
class QLabel;
class QCheckBox;

class ConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigDialog(QWidget *parent = nullptr);

    void loadSettings();
    void saveSettings();

signals:
    void settingsChanged();

private:
    void setupUi();

    // Danmaku display
    QFontComboBox *m_fontCombo     = nullptr;
    QSpinBox      *m_fontSize      = nullptr;
    QSpinBox      *m_httpPort      = nullptr;
    QSlider       *m_trackAreaSlider = nullptr;
    QLabel        *m_trackAreaLabel = nullptr;
    QComboBox     *m_speedCombo    = nullptr;
    QSpinBox      *m_rateLimit     = nullptr;
    QLineEdit     *m_logDirEdit    = nullptr;

    // Relay
    QLineEdit     *m_relayUrlEdit  = nullptr;
    QComboBox     *m_defaultMode   = nullptr;
    QCheckBox     *m_closeLocalInRelay = nullptr;
};
