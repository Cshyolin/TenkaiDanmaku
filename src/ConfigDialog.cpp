#include "ConfigDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QFontComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QSettings>
#include <QDialogButtonBox>
#include <QFontDatabase>

ConfigDialog::ConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("配置");
    setMinimumWidth(440);
    setupUi();
    loadSettings();
}

void ConfigDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    form->setSpacing(10);

    // ── Danmaku display ──────────────────────────────────────────────

    m_fontCombo = new QFontComboBox;
    m_fontCombo->setWritingSystem(QFontDatabase::SimplifiedChinese);
    form->addRow("弹幕字体:", m_fontCombo);

    m_fontSize = new QSpinBox;
    m_fontSize->setRange(12, 72);
    m_fontSize->setValue(24);
    form->addRow("字体大小:", m_fontSize);

    m_httpPort = new QSpinBox;
    m_httpPort->setRange(1024, 65535);
    m_httpPort->setValue(8080);
    m_httpPort->setToolTip("WebSocket 端口 = HTTP 端口 + 1");
    form->addRow("HTTP 起始端口:", m_httpPort);

    auto *trackLayout = new QHBoxLayout;
    m_trackAreaSlider = new QSlider(Qt::Horizontal);
    m_trackAreaSlider->setRange(10, 50);
    m_trackAreaSlider->setValue(30);
    m_trackAreaLabel = new QLabel("30%");
    m_trackAreaLabel->setFixedWidth(36);
    trackLayout->addWidget(m_trackAreaSlider);
    trackLayout->addWidget(m_trackAreaLabel);
    form->addRow("弹幕显示区域:", trackLayout);

    connect(m_trackAreaSlider, &QSlider::valueChanged, this, [this](int v) {
        m_trackAreaLabel->setText(QString("%1%").arg(v));
    });

    m_speedCombo = new QComboBox;
    m_speedCombo->addItem("慢  (8–12 秒)", QVariant::fromValue(QPair<int,int>(8000, 12000)));
    m_speedCombo->addItem("中  (5–8 秒)",  QVariant::fromValue(QPair<int,int>(5000, 8000)));
    m_speedCombo->addItem("快  (3–5 秒)",  QVariant::fromValue(QPair<int,int>(3000, 5000)));
    m_speedCombo->setCurrentIndex(1);
    form->addRow("弹幕速度:", m_speedCombo);

    m_rateLimit = new QSpinBox;
    m_rateLimit->setRange(1, 10);
    m_rateLimit->setValue(3);
    m_rateLimit->setSuffix(" 条/秒");
    form->addRow("速率限制:", m_rateLimit);

    auto *logLayout = new QHBoxLayout;
    m_logDirEdit = new QLineEdit("./logs");
    auto *browseBtn = new QPushButton("...");
    browseBtn->setFixedWidth(36);
    logLayout->addWidget(m_logDirEdit);
    logLayout->addWidget(browseBtn);
    form->addRow("日志保存位置:", logLayout);

    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, "选择日志目录", m_logDirEdit->text());
        if (!dir.isEmpty()) m_logDirEdit->setText(dir);
    });

    // ── Separator ────────────────────────────────────────────────────

    mainLayout->addLayout(form);

    auto *sep = new QLabel("── 公网中继 ──");
    sep->setStyleSheet("color: #e94560; font-weight: bold; margin-top: 8px;");
    mainLayout->addWidget(sep);

    auto *relayForm = new QFormLayout;
    relayForm->setSpacing(10);

    m_relayUrlEdit = new QLineEdit;
    m_relayUrlEdit->setPlaceholderText("wss://your-domain.com/ws");
    relayForm->addRow("中继服务器:", m_relayUrlEdit);

    m_defaultMode = new QComboBox;
    m_defaultMode->addItem("局域网直连");
    m_defaultMode->addItem("公网中继");
    relayForm->addRow("默认模式:", m_defaultMode);

    m_closeLocalInRelay = new QCheckBox("中继模式下关闭本地 HTTP/WS 服务");
    relayForm->addRow("", m_closeLocalInRelay);

    mainLayout->addLayout(relayForm);

    mainLayout->addSpacing(12);

    auto *note = new QLabel("日志路径、默认模式、中继服务器地址修改后需重启生效；其余设置即时应用。");
    note->setStyleSheet("color: #888; font-size: 11px;");
    note->setWordWrap(true);
    mainLayout->addWidget(note);

    mainLayout->addSpacing(8);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() { saveSettings(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ConfigDialog::loadSettings()
{
    QSettings s("TenkaiDanmaku", "TenkaiDanmaku");

    QFont f(s.value("fontFamily", "Microsoft YaHei").toString(),
            s.value("fontSize", 24).toInt());
    m_fontCombo->setCurrentFont(f);
    m_fontSize->setValue(s.value("fontSize", 24).toInt());
    m_httpPort->setValue(s.value("httpPort", 8080).toInt());
    m_trackAreaSlider->setValue(static_cast<int>(s.value("trackAreaRatio", 0.3).toDouble() * 100));
    m_rateLimit->setValue(s.value("rateLimit", 3).toInt());
    m_logDirEdit->setText(s.value("logDir", "./logs").toString());

    const int storedMin = s.value("animMinMs", 5000).toInt();
    const int storedMax = s.value("animMaxMs", 8000).toInt();
    for (int i = 0; i < m_speedCombo->count(); ++i) {
        auto range = m_speedCombo->itemData(i).value<QPair<int,int>>();
        if (range.first == storedMin && range.second == storedMax) {
            m_speedCombo->setCurrentIndex(i);
            break;
        }
    }

    // Relay settings
    m_relayUrlEdit->setText(s.value("relayUrl", "http://localhost:3000").toString());
    m_defaultMode->setCurrentIndex(s.value("defaultMode", 0).toInt());
    m_closeLocalInRelay->setChecked(s.value("closeLocalInRelay", false).toBool());
}

void ConfigDialog::saveSettings()
{
    QSettings s("TenkaiDanmaku", "TenkaiDanmaku");

    s.setValue("fontFamily",      m_fontCombo->currentFont().family());
    s.setValue("fontSize",        m_fontSize->value());
    s.setValue("httpPort",        m_httpPort->value());
    s.setValue("trackAreaRatio",  m_trackAreaSlider->value() / 100.0);
    s.setValue("rateLimit",       m_rateLimit->value());
    s.setValue("logDir",          m_logDirEdit->text());

    auto range = m_speedCombo->currentData().value<QPair<int,int>>();
    s.setValue("animMinMs", range.first);
    s.setValue("animMaxMs", range.second);

    // Relay
    s.setValue("relayUrl",          m_relayUrlEdit->text());
    s.setValue("defaultMode",       m_defaultMode->currentIndex());
    s.setValue("closeLocalInRelay", m_closeLocalInRelay->isChecked());

    emit settingsChanged();
}
