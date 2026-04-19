#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QGroupBox>
#include "streamup-ui.hpp"

class MouseSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit MouseSettingsDialog(QWidget *parent = nullptr);
	~MouseSettingsDialog();

private slots:
	void onApply();
	void onCancel();
	void onRefreshSources();

private:
	void setupUi();
	void loadSettings();
	void saveSettings();
	void populateSources(QComboBox *combo);

	// UI Elements
	QVBoxLayout *mainLayout;
	
	QComboBox *clickTargetCombo;
	QComboBox *scrollTargetCombo;
	QComboBox *positionTargetCombo;

	SwitchWidget *enableClicksCheck;
	SwitchWidget *enableScrollCheck;
	SwitchWidget *enablePositionCheck;
	SwitchWidget *enableLoggingCheck;
	SwitchWidget *startWithObsCheck;

	QSpinBox *mouseFpsSpin;

	QPushButton *applyBtn;
	QPushButton *cancelBtn;
	QPushButton *refreshBtn;
};
