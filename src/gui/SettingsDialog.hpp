#pragma once

#include "Controller.hpp"
#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(PowerManagerController *controller, QWidget *parent = nullptr);

private slots:
    void saveAndApply();
    void openHooksFolder();

private:
    void setupUi();
    QWidget* createProfilesTab();
    QWidget* createIdleTab();
    QWidget* createAutomationsTab();

    PowerManagerController *m_controller;

    // Profiles widgets
    struct ProfileWidgets {
        QComboBox *profile;
        QComboBox *fan;
        QComboBox *hz;
        QSpinBox *brightness;
        QSpinBox *kbd;
    };
    QMap<QString, ProfileWidgets> m_profileMap;

    // Idle widgets
    struct IdleWidgets {
        QSpinBox *dim;
        QSpinBox *dpms;
        QSpinBox *suspend;
    };
    QMap<QString, IdleWidgets> m_idleMap;

    // Automations widgets
    QLineEdit *m_cmdBattery;
    QLineEdit *m_cmdUsbc;
    QLineEdit *m_cmdAc;
    QSpinBox *m_lowPercent;
    QLineEdit *m_cmdLow;
    QSpinBox *m_critPercent;
    QLineEdit *m_cmdCrit;
};
