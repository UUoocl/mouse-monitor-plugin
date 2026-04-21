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

private:
	void setupUi();
	void loadSettings();
	void saveSettings();

	// UI Elements
	QVBoxLayout *mainLayout;

	SwitchWidget *enableClicksCheck;
	SwitchWidget *enableScrollCheck;
	SwitchWidget *enablePositionCheck;
	SwitchWidget *startWithObsCheck;

	QSpinBox *mouseFpsSpin;

	QPushButton *applyBtn;
	QPushButton *cancelBtn;
};
