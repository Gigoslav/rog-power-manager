#include "AboutDialog.hpp"
#include "Hardware.hpp"
#include "Translations.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent) {
    setupUi();
}

void AboutDialog::setupUi() {
    setWindowTitle(I18n::tr("about_win_title"));
    setMinimumWidth(540);
    setStyleSheet(
        "QDialog { background-color: #121317; color: #E5E7EB; font-family: 'Segoe UI', 'Ubuntu', sans-serif; }"
        "QGroupBox { font-weight: bold; border: 1px solid #2D3139; border-radius: 8px; margin-top: 12px; padding: 12px; background-color: #1A1C23; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; color: #38BDF8; font-size: 13px; }"
        "QLabel { color: #D1D5DB; font-size: 13px; }"
        "QLabel[isHeader='true'] { font-size: 18px; font-weight: bold; color: #F3F4F6; }"
        "QLabel[isSubheader='true'] { font-size: 13px; color: #9CA3AF; }"
        "QPushButton { background-color: #2563EB; color: white; border: none; border-radius: 6px; padding: 8px 24px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background-color: #1D4ED8; }"
        "QPushButton:pressed { background-color: #1E40AF; }"
    );

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

#ifndef APP_VERSION
#define APP_VERSION "0.4.3"
#endif
    auto header = new QLabel(I18n::tr("about_title").arg(QString(APP_VERSION) + " (C++ Native)"), this);
    header->setProperty("isHeader", true);
    layout->addWidget(header);

    auto subheader = new QLabel("Native C++ / Qt6 Core Replacement for KDE Plasma 6 PowerDevil & ASUS ROG Strix Control", this);
    subheader->setProperty("isSubheader", true);
    layout->addWidget(subheader);

    // Dev Group
    auto devGroup = new QGroupBox(I18n::tr("about_dev_group"), this);
    auto devLayout = new QFormLayout(devGroup);
    devLayout->setSpacing(8);

    devLayout->addRow(QString("<b>%1:</b>").arg(I18n::tr("about_author")),
                      new QLabel("Marcel Berghaus (Gigoslav) &lt;<a style='color:#38BDF8;' href='mailto:berghaus.marcel@gmail.com'>berghaus.marcel@gmail.com</a>&gt;", this));
    devLayout->addRow(QString("<b>%1:</b>").arg(I18n::tr("about_org")),
                      new QLabel("<b>Mountainhouse Software Solutions</b>", this));
    devLayout->addRow(QString("<b>%1:</b>").arg(I18n::tr("about_web")),
                      new QLabel("<a style='color:#38BDF8;' href='https://mountain-house.dev'>https://mountain-house.dev</a>", this));
    devLayout->addRow(QString("<b>%1:</b>").arg(I18n::tr("about_repo")),
                      new QLabel("<a style='color:#38BDF8;' href='https://github.com/Gigoslav/rog-power-manager'>https://github.com/Gigoslav/rog-power-manager</a>", this));
    devLayout->addRow(QString("<b>%1:</b>").arg(I18n::tr("about_license")),
                      new QLabel("GPL-3.0-or-later (<a style='color:#38BDF8;' href='https://www.gnu.org/licenses/gpl-3.0.html'>GNU General Public License v3</a>)", this));

    for (int i = 0; i < devLayout->rowCount(); ++i) {
        auto item = devLayout->itemAt(i, QFormLayout::FieldRole);
        if (auto lbl = qobject_cast<QLabel*>(item->widget())) {
            lbl->setOpenExternalLinks(true);
        }
    }
    layout->addWidget(devGroup);

    // Sys Group
    auto sysGroup = new QGroupBox(I18n::tr("about_sys_group"), this);
    auto sysLayout = new QFormLayout(sysGroup);
    sysLayout->setSpacing(8);

    QString src = Hardware::getPowerSource();
    QString srcDisp = (src == "ac") ? "240W Netzteil" : ((src == "usbc") ? "USB-C Power Delivery" : "Akkubetrieb");
    QString prof = Hardware::getCurrentPowerProfile();
    QString profDisp = I18n::tr("prof_" + prof);
    auto fans = Hardware::getFanTelemetry();
    QString fanStr = (fans.cpuRpm == 0 && fans.gpuRpm == 0) ? I18n::tr("fan_status_off") : QString("%1 RPM (CPU) | %2 RPM (GPU)").arg(fans.cpuRpm).arg(fans.gpuRpm);
    auto panel = Hardware::getPanelInfo();
    auto tel = Hardware::getPowerTelemetry(panel.currentRate);

    sysLayout->addRow("Stromquelle:", new QLabel(srcDisp, this));
    sysLayout->addRow("Aktives Profil:", new QLabel(QString("%1 (%2)").arg(profDisp, Hardware::currentFanMode), this));
    sysLayout->addRow(QString("%1:").arg(I18n::tr("about_fans")), new QLabel(fanStr, this));
    sysLayout->addRow("Bildschirm:", new QLabel(QString("%1 (%2) @ %3 Hz").arg(panel.name, panel.resolution).arg(panel.currentRate), this));
    sysLayout->addRow("Akku / Limit:", new QLabel(QString("%1% / Limit %2% (%3 W)").arg(tel.capacity).arg(tel.chargeLimit).arg(tel.batWatts, 0, 'f', 1), this));
    sysLayout->addRow("RTX 3060 Status:", new QLabel((tel.dgpuState == "suspended") ? I18n::tr("gpu_sleep") : I18n::tr("gpu_active"), this));

    layout->addWidget(sysGroup);

    auto btnBox = new QHBoxLayout();
    btnBox->addStretch();
    auto closeBtn = new QPushButton(I18n::tr("about_close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnBox->addWidget(closeBtn);
    layout->addLayout(btnBox);
}
