#include "mouse-settings-dialog.hpp"
#include <QDateTime>
#include <obs-module.h>

MouseSettingsDialog::MouseSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings();
}

MouseSettingsDialog::~MouseSettingsDialog()
{
}

void MouseSettingsDialog::setupUi()
{
    setWindowTitle("Mouse Monitor Settings");
    setMinimumSize(500, 450);
    setStyleSheet("QDialog { background: #1a1a1a; color: #f8fafc; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    // Global Settings
    QGroupBox *globalGroup = new QGroupBox("System Configuration", this);
    globalGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #38bdf8; border: 1px solid #334155; margin-top: 10px; padding-top: 15px; }");
    QVBoxLayout *globalLayout = new QVBoxLayout(globalGroup);

    enableClicksCheck = new QCheckBox("Broadcast Mouse Clicks (mouse/click)", this);
    globalLayout->addWidget(enableClicksCheck);

    enableScrollCheck = new QCheckBox("Broadcast Mouse Scroll (mouse/scroll)", this);
    globalLayout->addWidget(enableScrollCheck);

    enablePositionCheck = new QCheckBox("Broadcast Mouse Position (mouse/position)", this);
    globalLayout->addWidget(enablePositionCheck);

    enableGeneralRouteCheck = new QCheckBox("Enable General Route (mouse)", this);
    globalLayout->addWidget(enableGeneralRouteCheck);

    startWithObsCheck = new QCheckBox("Start with OBS (Automatically enable hooks)", this);
    globalLayout->addWidget(startWithObsCheck);

    QHBoxLayout *fpsLayout = new QHBoxLayout();
    fpsLayout->addWidget(new QLabel("Movement Throttle (FPS):", this));
    mouseFpsSpin = new QSpinBox(this);
    mouseFpsSpin->setRange(1, 144);
    mouseFpsSpin->setFixedWidth(80);
    fpsLayout->addWidget(mouseFpsSpin);
    fpsLayout->addStretch();
    globalLayout->addLayout(fpsLayout);

    mainLayout->addWidget(globalGroup);

    // Logging Section
    logContainer = new QWidget(this);
    QVBoxLayout *logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(0, 0, 0, 0);

    loggingCheck = new QCheckBox("Enable Real-time Activity Log", this);
    logLayout->addWidget(loggingCheck);

    logConsole = new QPlainTextEdit(this);
    logConsole->setReadOnly(true);
    logConsole->setStyleSheet("background: #0f172a; color: #4ade80; font-family: monospace; border: 1px solid #334155;");
    logConsole->setFixedHeight(120);
    logConsole->setVisible(false);
    logLayout->addWidget(logConsole);

    mainLayout->addWidget(logContainer);

    // Footer
    QHBoxLayout *footer = new QHBoxLayout();
    applyBtn = new QPushButton("Apply & Close", this);
    applyBtn->setFixedWidth(120);
    applyBtn->setStyleSheet("QPushButton { background: #38bdf8; color: #0f172a; font-weight: bold; border-radius: 4px; padding: 6px; } QPushButton:hover { background: #7dd3fc; }");
    
    cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setFixedWidth(80);
    
    footer->addStretch();
    footer->addWidget(cancelBtn);
    footer->addWidget(applyBtn);
    mainLayout->addLayout(footer);

    connect(applyBtn, &QPushButton::clicked, this, &MouseSettingsDialog::onApply);
    connect(cancelBtn, &QPushButton::clicked, this, &MouseSettingsDialog::onCancel);
    connect(loggingCheck, &QCheckBox::toggled, this, &MouseSettingsDialog::ToggleLog);
}

void MouseSettingsDialog::loadSettings()
{
    char *path = obs_module_config_path("mouse-monitor-settings.json");
    if (!path) return;

    obs_data_t *settings = obs_data_create_from_json_file(path);
    if (settings) {
        enableClicksCheck->setChecked(obs_data_get_bool(settings, "sendClicks"));
        enableScrollCheck->setChecked(obs_data_get_bool(settings, "sendScroll"));
        enablePositionCheck->setChecked(obs_data_get_bool(settings, "sendPosition"));
        enableGeneralRouteCheck->setChecked(obs_data_get_bool(settings, "sendGeneralRoute"));
        
        int fps = (int)obs_data_get_int(settings, "mouseFps");
        if (fps == 0) fps = 50;
        mouseFpsSpin->setValue(fps);
        
        startWithObsCheck->setChecked(obs_data_get_bool(settings, "startWithObs"));
        loggingCheck->setChecked(obs_data_get_bool(settings, "enableLogging"));
        logConsole->setVisible(loggingCheck->isChecked());

        obs_data_release(settings);
    } else {
        // Defaults
        enableClicksCheck->setChecked(true);
        enableScrollCheck->setChecked(true);
        enablePositionCheck->setChecked(true);
        enableGeneralRouteCheck->setChecked(true);
        mouseFpsSpin->setValue(50);
        startWithObsCheck->setChecked(true);
    }
    bfree(path);
}

void MouseSettingsDialog::onApply()
{
    saveSettings();
    accept();
}

void MouseSettingsDialog::onCancel()
{
    reject();
}

void MouseSettingsDialog::saveSettings()
{
    obs_data_t *settings = obs_data_create();
    obs_data_set_bool(settings, "sendClicks", enableClicksCheck->isChecked());
    obs_data_set_bool(settings, "sendScroll", enableScrollCheck->isChecked());
    obs_data_set_bool(settings, "sendPosition", enablePositionCheck->isChecked());
    obs_data_set_bool(settings, "sendGeneralRoute", enableGeneralRouteCheck->isChecked());
    obs_data_set_int(settings, "mouseFps", mouseFpsSpin->value());
    obs_data_set_bool(settings, "startWithObs", startWithObsCheck->isChecked());
    obs_data_set_bool(settings, "enableLogging", loggingCheck->isChecked());

    char *path = obs_module_config_path("mouse-monitor-settings.json");
    if (path) {
        obs_data_save_json(settings, path);
        bfree(path);
    }
    
    // In Mouse Monitor, we need to bridge these to the globals in the .cpp
    extern bool sendClicks, sendScroll, sendPosition, startWithObs, enableLogging, sendGeneralRoute;
    extern uint64_t mouseFps;
    extern std::atomic<uint64_t> moveThrottleMs;

    sendClicks = enableClicksCheck->isChecked();
    sendScroll = enableScrollCheck->isChecked();
    sendPosition = enablePositionCheck->isChecked();
    sendGeneralRoute = enableGeneralRouteCheck->isChecked();
    startWithObs = startWithObsCheck->isChecked();
    enableLogging = loggingCheck->isChecked();
    mouseFps = mouseFpsSpin->value();
    moveThrottleMs.store(1000 / mouseFps);

    obs_data_release(settings);
}

void MouseSettingsDialog::ToggleLog(bool checked)
{
    logConsole->setVisible(checked);
}

void MouseSettingsDialog::AppendLog(const QString &msg)
{
    if (!loggingCheck->isChecked()) return;
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    logConsole->appendPlainText(QString("[%1] %2").arg(timestamp, msg));
}
