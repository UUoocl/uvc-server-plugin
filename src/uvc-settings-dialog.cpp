#include "uvc-settings-dialog.hpp"
#include <QHBoxLayout>
#include <QGroupBox>

UvcSettingsDialog::UvcSettingsDialog(QWidget *parent) : QDialog(parent)
{
	blog(LOG_INFO, "[UVC Server] Settings dialog opening");
	SetupUI();
	RefreshDeviceList();

	GetUvcManager().logCallback = [this](const std::string &msg) {
		QMetaObject::invokeMethod(this, "AppendLog", Qt::QueuedConnection,
					  Q_ARG(QString, QString::fromStdString(msg)));
	};
}

UvcSettingsDialog::~UvcSettingsDialog()
{
	GetUvcManager().logCallback = nullptr;
}

void UvcSettingsDialog::SetupUI()
{
	setWindowTitle("UVC Camera Control Settings");
	setMinimumSize(500, 600);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);

	// Device List Group
	QGroupBox *deviceGroup = new QGroupBox("Available UVC Cameras", this);
	QVBoxLayout *groupLayout = new QVBoxLayout(deviceGroup);

	QScrollArea *scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	QWidget *scrollContent = new QWidget();
	deviceListLayout = new QVBoxLayout(scrollContent);
	deviceListLayout->setAlignment(Qt::AlignTop);
	scrollArea->setWidget(scrollContent);

	groupLayout->addWidget(scrollArea);

	QPushButton *refreshBtn = new QPushButton("Refresh Devices", this);
	connect(refreshBtn, &QPushButton::clicked, this, &UvcSettingsDialog::RefreshDeviceList);
	groupLayout->addWidget(refreshBtn);

	mainLayout->addWidget(deviceGroup);

	startWithObsCb = new QCheckBox("Start with OBS (Automatically enable saved cameras)", this);
	startWithObsCb->setChecked(GetUvcManager().ShouldStartWithObs());
	mainLayout->addWidget(startWithObsCb);

	// Log Console
	QGroupBox *logGroup = new QGroupBox("Activity Log", this);
	QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
	logConsole = new QTextEdit(this);
	logConsole->setReadOnly(true);
	logLayout->addWidget(logConsole);
	mainLayout->addWidget(logGroup);

	// Buttons
	QHBoxLayout *btnLayout = new QHBoxLayout();
	QPushButton *closeBtn = new QPushButton("Close", this);
	connect(closeBtn, &QPushButton::clicked, this, &UvcSettingsDialog::SaveAndClose);
	btnLayout->addStretch();
	btnLayout->addWidget(closeBtn);
	mainLayout->addLayout(btnLayout);
}

void UvcSettingsDialog::RefreshDeviceList()
{
	GetUvcManager().RefreshDevices();

	// Clear current widgets
	QLayoutItem *child;
	while ((child = deviceListLayout->takeAt(0)) != nullptr) {
		delete child->widget();
		delete child;
	}
	deviceWidgets.clear();

	auto devices = GetUvcManager().GetDevices();
	for (auto &dev : devices) {
		QWidget *row = new QWidget();
		QHBoxLayout *rowLayout = new QHBoxLayout(row);

		QCheckBox *cb = new QCheckBox("Enabled", row);
		cb->setChecked(dev->enabled);

		QLabel *nameLabel = new QLabel(QString::fromStdString(dev->name), row);
		nameLabel->setMinimumWidth(200);
		nameLabel->setStyleSheet("font-weight: bold;");

		QLineEdit *aliasEdit = new QLineEdit(QString::fromStdString(dev->user_name), row);
		aliasEdit->setPlaceholderText("Alias (Optional)");
		aliasEdit->setFixedWidth(150);

		QLabel *info = new QLabel(QString("VID:%1 PID:%2")
						  .arg(dev->vendor_id, 4, 16, QChar('0'))
						  .arg(dev->product_id, 4, 16, QChar('0')),
					  row);
		info->setStyleSheet("color: gray;");

		rowLayout->addWidget(cb);
		rowLayout->addWidget(nameLabel);
		rowLayout->addWidget(aliasEdit);
		rowLayout->addStretch();
		rowLayout->addWidget(info);

		deviceListLayout->addWidget(row);

		DeviceWidget dw;
		dw.dev = dev;
		dw.enabled = cb;
		dw.alias = aliasEdit;
		deviceWidgets.push_back(dw);

		// Immediate logging and activation for UX feedback
		connect(cb, &QCheckBox::toggled, [dev](bool checked) {
			blog(LOG_INFO, "[UVC Server] enabled pressed, broadcasting device setting for '%s'...",
			     dev->name.c_str());
			GetUvcManager().SetDeviceEnabled(dev->name, checked);
		});
	}
}

void UvcSettingsDialog::SaveAndClose()
{
	for (auto &dw : deviceWidgets) {
		dw.dev->user_name = dw.alias->text().toStdString();
		GetUvcManager().SetDeviceEnabled(dw.dev->name, dw.enabled->isChecked());
	}
	GetUvcManager().SetStartWithObs(startWithObsCb->isChecked());
	GetUvcManager().SaveConfig();
	accept();
}

void UvcSettingsDialog::AppendLog(const QString &message)
{
	logConsole->append(message);
}
