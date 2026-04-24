#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QPlainTextEdit>

class MouseSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit MouseSettingsDialog(QWidget *parent = nullptr);
    ~MouseSettingsDialog();

public slots:
    void AppendLog(const QString &msg);

private slots:
    void onApply();
    void onCancel();
    void ToggleLog(bool checked);

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    // UI Elements
    QCheckBox *enableClicksCheck;
    QCheckBox *enableScrollCheck;
    QCheckBox *enablePositionCheck;
    QCheckBox *enableGeneralRouteCheck;
    QCheckBox *startWithObsCheck;
    QCheckBox *loggingCheck;

    QSpinBox *mouseFpsSpin;

    QPlainTextEdit *logConsole;
    QWidget *logContainer;

    QPushButton *applyBtn;
    QPushButton *cancelBtn;
};
