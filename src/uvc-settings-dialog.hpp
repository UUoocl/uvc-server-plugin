#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <vector>
#include "uvc-manager.hpp"

class UvcSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit UvcSettingsDialog(QWidget *parent = nullptr);
	~UvcSettingsDialog();

private slots:
	void RefreshDeviceList();
	void SaveAndClose();
	void AppendLog(const QString &message);

private:
	void SetupUI();
	
	QVBoxLayout *deviceListLayout;
	QTextEdit *logConsole;
	struct DeviceWidget {
		UvcDevicePtr dev;
		QCheckBox *enabled;
		QLineEdit *alias;
	};
	std::vector<DeviceWidget> deviceWidgets;
	QCheckBox *startWithObsCb;
};
