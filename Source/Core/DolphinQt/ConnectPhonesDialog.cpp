// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/ConnectPhonesDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSvgRenderer>
#include <QVBoxLayout>

#include "DolphinQt/PartyPadProcess.h"
#include "DolphinQt/Settings.h"

ConnectPhonesDialog::ConnectPhonesDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Connect Phones — WiiTogether"));
  setMinimumWidth(480);
  m_server = new PartyPadProcess(this);
  auto* layout = new QVBoxLayout(this);
  m_status = new QLabel;
  m_status->setTextFormat(Qt::PlainText);
  m_status->setWordWrap(true);
  layout->addWidget(m_status);
  auto* folder_row = new QHBoxLayout;
  m_directory = new QLineEdit;
  m_directory->setPlaceholderText(tr("PartyPad source folder"));
  QString directory =
      Settings::GetQSettings().value(QStringLiteral("wiitogether/partypad_path")).toString();
  if (directory.isEmpty())
  {
    QDir candidate(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 10; ++i)
    {
      if (QFileInfo::exists(candidate.filePath(QStringLiteral("partypad/server.py"))))
      {
        directory = candidate.filePath(QStringLiteral("partypad"));
        break;
      }
      if (!candidate.cdUp())
        break;
    }
  }
  m_directory->setText(directory);
  m_browse = new QPushButton(tr("Browse…"));
  folder_row->addWidget(m_directory);
  folder_row->addWidget(m_browse);
  layout->addLayout(folder_row);
  m_qr = new QLabel;
  m_qr->setFixedSize(260, 260);
  // A white quiet zone keeps the QR readable with dark desktop themes.
  m_qr->setStyleSheet(QStringLiteral("background: white; padding: 12px;"));
  layout->addWidget(m_qr, 0, Qt::AlignHCenter);
  m_url = new QLineEdit;
  m_url->setReadOnly(true);
  m_copy = new QPushButton(tr("Copy URL"));
  auto* url_row = new QHBoxLayout;
  url_row->addWidget(m_url);
  url_row->addWidget(m_copy);
  layout->addLayout(url_row);
  for (int i = 0; i < 4; ++i)
  {
    m_players[i] = new QLabel;
    layout->addWidget(m_players[i]);
  }
  auto* instructions = new QLabel(tr(
      "Accept the certificate warning and allow motion access on each phone. "
      "Player numbers identify PartyPad/DSU slots; use Dolphin’s existing controller mappings."));
  instructions->setWordWrap(true);
  layout->addWidget(instructions);
  auto* buttons = new QHBoxLayout;
  m_retry = new QPushButton(tr("Retry / Start"));
  m_stop = new QPushButton(tr("Stop"));
  auto* close = new QPushButton(tr("Close"));
  buttons->addWidget(m_retry);
  buttons->addWidget(m_stop);
  buttons->addStretch();
  buttons->addWidget(close);
  layout->addLayout(buttons);
  connect(close, &QPushButton::clicked, this, &QDialog::hide);
  connect(m_retry, &QPushButton::clicked, this, &ConnectPhonesDialog::Start);
  connect(m_stop, &QPushButton::clicked, m_server, &PartyPadProcess::Stop);
  connect(m_copy, &QPushButton::clicked, this,
          [this] { QApplication::clipboard()->setText(m_url->text()); });
  connect(m_browse, &QPushButton::clicked, this, [this] {
    const QString path =
        QFileDialog::getExistingDirectory(this, tr("Select PartyPad folder"), m_directory->text());
    if (!path.isEmpty())
      m_directory->setText(path);
  });
  connect(m_server, &PartyPadProcess::Changed, this, &ConnectPhonesDialog::Refresh);
  Refresh();
}

void ConnectPhonesDialog::Start()
{
  if (m_server->IsRunning())
    return;
  const QString path = QDir(m_directory->text()).absolutePath();
  Settings::GetQSettings().setValue(QStringLiteral("wiitogether/partypad_path"), path);
  m_server->Start(path);
}

void ConnectPhonesDialog::Refresh()
{
  m_status->setText(m_server->Status());
  m_url->setText(m_server->JoinUrl());
  m_copy->setEnabled(!m_url->text().isEmpty());
  const bool ready = !m_server->JoinUrl().isEmpty();
  if (ready && m_url->text() != m_qr->property("join_url").toString())
  {
    QPixmap image(260, 260);
    image.fill(Qt::white);
    QPainter painter(&image);
    QSvgRenderer renderer(m_server->QrPath());
    renderer.render(&painter, QRectF(12, 12, 236, 236));
    painter.end();
    m_qr->setPixmap(image);
    m_qr->setProperty("join_url", m_url->text());
  }
  if (!ready)
    m_qr->setProperty("join_url", QString{});
  m_qr->setVisible(ready);
  m_retry->setEnabled(!m_server->IsRunning());
  m_stop->setEnabled(m_server->IsRunning());
  m_directory->setEnabled(!m_server->IsRunning());
  m_browse->setEnabled(!m_server->IsRunning());
  for (int i = 0; i < 4; ++i)
    m_players[i]->setText(
        tr("Player %1 — %2")
            .arg(i + 1)
            .arg(m_server->IsPlayerConnected(i + 1) ? tr("Connected") : tr("Waiting")));
}
