#include "uvc-settings-dialog.hpp"
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>

UvcDeviceRow::UvcDeviceRow(UvcDevicePtr device, QWidget *parent)
    : QFrame(parent), device(device)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    bool connected = device->isOpened;
    statusLabel = new QLabel(connected ? "●" : "○", this);
    statusLabel->setStyleSheet(connected ? "color: #4ade80; font-size: 14px;" : "color: #475569; font-size: 14px;");
    layout->addWidget(statusLabel);

    nameLabel = new QLabel(QString::fromStdString(device->name), this);
    nameLabel->setStyleSheet("font-weight: 600; color: #f8fafc;");
    layout->addWidget(nameLabel, 1);

    enabledCheck = new QCheckBox("Enabled", this);
    enabledCheck->setChecked(device->enabled);
    layout->addWidget(enabledCheck);

    layout->addWidget(new QLabel("Alias:", this));
    aliasEdit = new QLineEdit(this);
    aliasEdit->setText(QString::fromStdString(device->user_name));
    aliasEdit->setPlaceholderText("Alias...");
    aliasEdit->setFixedWidth(120);
    layout->addWidget(aliasEdit);

    QLabel *idLabel = new QLabel(QString("VID:%1 PID:%2")
        .arg(device->vendor_id, 4, 16, QChar('0'))
        .arg(device->product_id, 4, 16, QChar('0')), this);
    idLabel->setStyleSheet("color: #64748b; font-family: monospace; font-size: 10px;");
    layout->addWidget(idLabel);

    setStyleSheet("UvcDeviceRow { background: #2a2a2a; border-radius: 4px; border: 1px solid #3a3a3a; }");
    setFixedHeight(40);

    connect(enabledCheck, &QCheckBox::toggled, [this](bool checked) {
        GetUvcManager().SetDeviceEnabled(this->device->name, checked);
    });
}

bool UvcDeviceRow::IsEnabled() const { return enabledCheck->isChecked(); }
QString UvcDeviceRow::GetAlias() const { return aliasEdit->text(); }

UvcSettingsDialog::UvcSettingsDialog(QWidget *parent)
    : QDialog(parent), mgr(GetUvcManager())
{
    SetupUI();
    RefreshDeviceList();

    mgr.logCallback = [this](const std::string &msg) {
        QMetaObject::invokeMethod(this, "AppendLog", Qt::QueuedConnection,
                                Q_ARG(QString, QString::fromStdString(msg)));
    };
}

UvcSettingsDialog::~UvcSettingsDialog()
{
    mgr.logCallback = nullptr;
}

void UvcSettingsDialog::SetupUI()
{
    setWindowTitle("UVC Server Settings");
    setMinimumSize(600, 500);
    setStyleSheet("QDialog { background: #1a1a1a; color: #f8fafc; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    // Global Settings
    QGroupBox *globalGroup = new QGroupBox("System Configuration", this);
    globalGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #38bdf8; border: 1px solid #334155; margin-top: 10px; padding-top: 15px; }");
    QVBoxLayout *globalLayout = new QVBoxLayout(globalGroup);

    enableAllCheck = new QCheckBox("Enable UVC System", this);
    enableAllCheck->setChecked(mgr.IsGlobalEnabled());
    globalLayout->addWidget(enableAllCheck);

    autoStartCheck = new QCheckBox("Start with OBS (Automatically enable devices)", this);
    autoStartCheck->setChecked(mgr.ShouldStartWithObs());
    globalLayout->addWidget(autoStartCheck);

    mainLayout->addWidget(globalGroup);

    // Device List
    QLabel *listHeader = new QLabel("Detected UVC Cameras", this);
    listHeader->setStyleSheet("font-weight: bold; font-size: 14px; color: #94a3b8;");
    mainLayout->addWidget(listHeader);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget *scrollContent = new QWidget();
    devicesLayout = new QVBoxLayout(scrollContent);
    devicesLayout->setContentsMargins(0, 0, 0, 0);
    devicesLayout->setSpacing(8);
    devicesLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(scrollContent);
    mainLayout->addWidget(scroll, 1);

    // Logging Section
    logContainer = new QWidget(this);
    QVBoxLayout *logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *logHeader = new QHBoxLayout();
    logCheck = new QCheckBox("Enable Real-time Activity Log", this);
    logCheck->setChecked(mgr.IsLoggingEnabled());
    logHeader->addWidget(logCheck);
    logHeader->addStretch();
    
    QPushButton *refreshBtn = new QPushButton("Refresh Devices", this);
    refreshBtn->setFixedWidth(120);
    logHeader->addWidget(refreshBtn);
    logLayout->addLayout(logHeader);

    logConsole = new QPlainTextEdit(this);
    logConsole->setReadOnly(true);
    logConsole->setStyleSheet("background: #0f172a; color: #4ade80; font-family: monospace; border: 1px solid #334155;");
    logConsole->setFixedHeight(120);
    logConsole->setVisible(!mgr.IsLogCollapsed());
    logLayout->addWidget(logConsole);

    mainLayout->addWidget(logContainer);

    // Footer
    QHBoxLayout *footer = new QHBoxLayout();
    QPushButton *saveBtn = new QPushButton("Apply & Close", this);
    saveBtn->setFixedWidth(120);
    saveBtn->setStyleSheet("QPushButton { background: #38bdf8; color: #0f172a; font-weight: bold; border-radius: 4px; padding: 6px; } QPushButton:hover { background: #7dd3fc; }");
    
    footer->addStretch();
    footer->addWidget(saveBtn);
    mainLayout->addLayout(footer);

    // Connections
    connect(refreshBtn, &QPushButton::clicked, this, &UvcSettingsDialog::RefreshDeviceList);
    connect(saveBtn, &QPushButton::clicked, this, &UvcSettingsDialog::SaveSettings);
    connect(logCheck, &QCheckBox::toggled, this, &UvcSettingsDialog::ToggleLog);
    connect(enableAllCheck, &QCheckBox::toggled, [](bool checked) {
        GetUvcManager().SetGlobalEnabled(checked);
    });
}

void UvcSettingsDialog::RefreshDeviceList()
{
    mgr.RefreshDevices();

    for (auto row : deviceRows) {
        devicesLayout->removeWidget(row);
        row->deleteLater();
    }
    deviceRows.clear();

    auto devices = mgr.GetDevices();
    for (auto &dev : devices) {
        UvcDeviceRow *row = new UvcDeviceRow(dev, this);
        devicesLayout->addWidget(row);
        deviceRows.push_back(row);
    }
}

void UvcSettingsDialog::SaveSettings()
{
    for (auto row : deviceRows) {
        UvcDevicePtr dev = row->GetDevice();
        dev->user_name = row->GetAlias().toStdString();
        mgr.SetDeviceEnabled(dev->name, row->IsEnabled());
    }

    mgr.SetStartWithObs(autoStartCheck->isChecked());
    mgr.SetLoggingEnabled(logCheck->isChecked());
    mgr.SetLogCollapsed(!logConsole->isVisible());
    mgr.SaveConfig();
    accept();
}

void UvcSettingsDialog::AppendLog(const QString &message)
{
    if (!logCheck->isChecked()) return;
    
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    logConsole->appendPlainText(QString("[%1] %2").arg(timestamp, message));
    
    if (logConsole->blockCount() > 100) {
        QTextCursor cursor = logConsole->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.select(QTextCursor::LineUnderCursor);
        cursor.removeSelectedText();
        cursor.deleteChar(); // newline
    }
}

void UvcSettingsDialog::ToggleLog(bool checked)
{
    logConsole->setVisible(checked);
    mgr.SetLoggingEnabled(checked);
}
