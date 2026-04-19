#include "mouse-settings-dialog.hpp"
#include "mouse-monitor-plugin.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>

MouseSettingsDialog::MouseSettingsDialog(QWidget *parent) : QDialog(parent)
{
	setupUi();
	loadSettings();
}

MouseSettingsDialog::~MouseSettingsDialog() {}

void MouseSettingsDialog::setupUi()
{
	DialogChrome chrome = ApplyDialogChrome(this, obs_module_text("Settings.Title"));
	mainLayout = chrome.contentLayout;

	// Targets Group
	QGroupBox *targetsGb = new QGroupBox(obs_module_text("Settings.Group.Targets"));
	targetsGb->setStyleSheet(GetGroupBoxStyle());
	QVBoxLayout *targetsLay = new QVBoxLayout(targetsGb);
	targetsLay->setSpacing(10);

	auto addTargetRow = [&](const QString &labelText, QComboBox **combo) {
		QHBoxLayout *row = new QHBoxLayout();
		QLabel *lbl = new QLabel(labelText);
		lbl->setStyleSheet(GetLabelStyle());
		*combo = new QComboBox();
		(*combo)->setStyleSheet(GetComboBoxStyle());
		populateSources(*combo);
		row->addWidget(lbl);
		row->addWidget(*combo, 1);
		targetsLay->addLayout(row);
	};

	addTargetRow(obs_module_text("Settings.Label.ClickTarget"), &clickTargetCombo);
	addTargetRow(obs_module_text("Settings.Label.ScrollTarget"), &scrollTargetCombo);
	addTargetRow(obs_module_text("Settings.Label.PositionTarget"), &positionTargetCombo);

	mainLayout->addWidget(targetsGb);

	// Monitoring Options Group
	QGroupBox *optionsGb = new QGroupBox(obs_module_text("Settings.Group.Monitoring"));
	optionsGb->setStyleSheet(GetGroupBoxStyle());
	QVBoxLayout *optionsLay = new QVBoxLayout(optionsGb);
	optionsLay->setSpacing(8);

	enableClicksCheck = new SwitchWidget(obs_module_text("Settings.Checkbox.EnableClicks"));
	enableScrollCheck = new SwitchWidget(obs_module_text("Settings.Checkbox.EnableScroll"));
	enablePositionCheck = new SwitchWidget(obs_module_text("Settings.Checkbox.EnablePosition"));
	enableLoggingCheck = new SwitchWidget(obs_module_text("Settings.Checkbox.EnableLogging"));
	startWithObsCheck = new SwitchWidget(obs_module_text("Settings.Checkbox.StartWithObs"));

	optionsLay->addWidget(enableClicksCheck);
	optionsLay->addWidget(enableScrollCheck);
	optionsLay->addWidget(enablePositionCheck);
	optionsLay->addWidget(enableLoggingCheck);
	optionsLay->addWidget(startWithObsCheck);

	QHBoxLayout *fpsRow = new QHBoxLayout();
	QLabel *fpsLbl = new QLabel(obs_module_text("Settings.Label.MouseFps"));
	fpsLbl->setStyleSheet(GetLabelStyle());
	mouseFpsSpin = new QSpinBox();
	mouseFpsSpin->setRange(1, 120);
	mouseFpsSpin->setStyleSheet(GetSpinBoxStyle());
	fpsRow->addWidget(fpsLbl);
	fpsRow->addWidget(mouseFpsSpin);
	fpsRow->addStretch();
	optionsLay->addLayout(fpsRow);

	mainLayout->addWidget(optionsGb);
	mainLayout->addStretch();

	// Footer buttons
	refreshBtn = CreateStyledButton(obs_module_text("Refresh Sources"), "default");
	applyBtn = CreateStyledButton(obs_module_text("Settings.Button.Apply"), "primary");
	cancelBtn = CreateStyledButton(obs_module_text("Settings.Button.Close"), "default");

	QHBoxLayout *btnLay = new QHBoxLayout();
	btnLay->addWidget(refreshBtn);
	btnLay->addStretch();
	btnLay->addWidget(applyBtn);
	btnLay->addWidget(cancelBtn);
	chrome.footerLayout->addLayout(btnLay);

	connect(applyBtn, &QPushButton::clicked, this, &MouseSettingsDialog::onApply);
	connect(cancelBtn, &QPushButton::clicked, this, &MouseSettingsDialog::onCancel);
	connect(refreshBtn, &QPushButton::clicked, this, &MouseSettingsDialog::onRefreshSources);
}

void MouseSettingsDialog::populateSources(QComboBox *combo)
{
	QString current = combo->currentText();
	combo->clear();
	combo->addItem("None");

	auto enum_proc = [](void *data, obs_source_t *source) {
		QComboBox *comboBox = static_cast<QComboBox *>(data);
		const char *id = obs_source_get_id(source);
		if (strcmp(id, "browser_source") == 0) {
			comboBox->addItem(obs_source_get_name(source));
		}
		return true;
	};

	obs_enum_sources(enum_proc, combo);

	int index = combo->findText(current);
	if (index != -1) combo->setCurrentIndex(index);
}

void MouseSettingsDialog::loadSettings()
{
	obs_data_t *settings = SaveLoadSettingsCallback(nullptr, false);
	if (settings) {
		clickTargetCombo->setCurrentText(obs_data_get_string(settings, "clickTarget"));
		scrollTargetCombo->setCurrentText(obs_data_get_string(settings, "scrollTarget"));
		positionTargetCombo->setCurrentText(obs_data_get_string(settings, "positionTarget"));

		enableClicksCheck->setChecked(obs_data_get_bool(settings, "sendClicks"));
		enableScrollCheck->setChecked(obs_data_get_bool(settings, "sendScroll"));
		enablePositionCheck->setChecked(obs_data_get_bool(settings, "sendPosition"));
		enableLoggingCheck->setChecked(obs_data_get_bool(settings, "enableLogging"));
		startWithObsCheck->setChecked(obs_data_get_bool(settings, "startWithObs"));

		int fps = (int)obs_data_get_int(settings, "mouseFps");
		mouseFpsSpin->setValue(fps > 0 ? fps : 50);

		obs_data_release(settings);
	}
}

void MouseSettingsDialog::saveSettings()
{
	obs_data_t *settings = obs_data_create();

	obs_data_set_string(settings, "clickTarget", clickTargetCombo->currentText().toUtf8().constData());
	obs_data_set_string(settings, "scrollTarget", scrollTargetCombo->currentText().toUtf8().constData());
	obs_data_set_string(settings, "positionTarget", positionTargetCombo->currentText().toUtf8().constData());

	obs_data_set_bool(settings, "sendClicks", enableClicksCheck->isChecked());
	obs_data_set_bool(settings, "sendScroll", enableScrollCheck->isChecked());
	obs_data_set_bool(settings, "sendPosition", enablePositionCheck->isChecked());
	obs_data_set_bool(settings, "enableLogging", enableLoggingCheck->isChecked());
	obs_data_set_bool(settings, "startWithObs", startWithObsCheck->isChecked());
	obs_data_set_int(settings, "mouseFps", mouseFpsSpin->value());

	SaveLoadSettingsCallback(settings, true);
	obs_data_release(settings);
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

void MouseSettingsDialog::onRefreshSources()
{
	populateSources(clickTargetCombo);
	populateSources(scrollTargetCombo);
	populateSources(positionTargetCombo);
}
