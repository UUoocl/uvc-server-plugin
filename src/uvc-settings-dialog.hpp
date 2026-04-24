#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFrame>
#include <vector>
#include "uvc-manager.hpp"

class UvcDeviceRow : public QFrame {
    Q_OBJECT

public:
    explicit UvcDeviceRow(UvcDevicePtr device, QWidget *parent = nullptr);
    
    UvcDevicePtr GetDevice() const { return device; }
    bool IsEnabled() const;
    QString GetAlias() const;

private:
    UvcDevicePtr device;
    QLabel *statusLabel;
    QLabel *nameLabel;
    QCheckBox *enabledCheck;
    QLineEdit *aliasEdit;
};

class UvcSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit UvcSettingsDialog(QWidget *parent = nullptr);
    ~UvcSettingsDialog();

private slots:
    void RefreshDeviceList();
    void SaveSettings();
    void AppendLog(const QString &message);
    void ToggleLog(bool checked);

private:
    void SetupUI();

    UvcManager &mgr;
    QVBoxLayout *devicesLayout;
    std::vector<UvcDeviceRow*> deviceRows;
    
    QCheckBox *enableAllCheck;
    QCheckBox *autoStartCheck;
    QCheckBox *logCheck;
    QPlainTextEdit *logConsole;
    QWidget *logContainer;
};
