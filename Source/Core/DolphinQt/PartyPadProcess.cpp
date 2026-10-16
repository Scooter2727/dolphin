// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/PartyPadProcess.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

PartyPadProcess::PartyPadProcess(QObject* parent) : QObject(parent)
{
  m_status = tr("Ready to connect phones.");
  m_poll.setInterval(300);
  m_timeout.setSingleShot(true);
  m_kill_timer.setSingleShot(true);
  connect(&m_poll, &QTimer::timeout, this, &PartyPadProcess::Poll);
  connect(&m_timeout, &QTimer::timeout, this, [this] {
    Stop();
    SetStatus(
        tr("PartyPad startup timed out. Check the server dependencies and network settings."));
  });
  connect(&m_kill_timer, &QTimer::timeout, &m_process, &QProcess::kill);
  m_process.setProcessChannelMode(QProcess::MergedChannels);
  connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
    m_output += QString::fromUtf8(m_process.readAllStandardOutput());
    m_output = m_output.right(4000);
  });
  connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart)
    {
      m_timeout.stop();
      m_poll.stop();
      SetStatus(tr("Could not start PartyPad: %1").arg(m_process.errorString()));
    }
  });
  connect(&m_process, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
    m_timeout.stop();
    m_kill_timer.stop();
    m_poll.stop();
    m_url.clear();
    m_players.clear();
    SetStatus(m_stopping ? m_status : tr("PartyPad exited.\n%1").arg(m_output));
  });
}

PartyPadProcess::~PartyPadProcess()
{
  Stop();
  if (IsRunning() && !m_process.waitForFinished(3000))
  {
    m_process.kill();
    m_process.waitForFinished(1000);
  }
}

bool PartyPadProcess::IsRunning() const
{
  return m_process.state() != QProcess::NotRunning;
}

QString PartyPadProcess::QrPath() const
{
  return m_temp.filePath(QStringLiteral("state.svg"));
}

bool PartyPadProcess::IsPlayerConnected(int player) const
{
  return m_players.contains(player);
}

void PartyPadProcess::SetStatus(const QString& status)
{
  m_status = status;
  emit Changed();
}

void PartyPadProcess::Start(const QString& directory)
{
  if (IsRunning())
    return;
  const QDir root(directory);
#ifdef _WIN32
  const QString python = root.filePath(QStringLiteral(".venv/Scripts/python.exe"));
#else
  const QString python = root.filePath(QStringLiteral(".venv/bin/python"));
#endif
  if (!m_temp.isValid() || !QFileInfo::exists(python) ||
      !QFileInfo::exists(root.filePath(QStringLiteral("partypad.py"))))
  {
    SetStatus(tr("Select a PartyPad source folder with an installed .venv. Run uv sync in that "
                 "folder first."));
    return;
  }
  QFile::remove(m_temp.filePath(QStringLiteral("stop")));
  QFile::remove(m_temp.filePath(QStringLiteral("state.json")));
  QFile::remove(QrPath());
  m_output.clear();
  m_url.clear();
  m_players.clear();
  m_stopping = false;
  m_process.setWorkingDirectory(root.absolutePath());
  m_process.start(python,
                  {QStringLiteral("-u"), root.absoluteFilePath(QStringLiteral("partypad.py")),
                   QStringLiteral("bridge"), QStringLiteral("--system"), QStringLiteral("wii"),
                   QStringLiteral("--state-file"), m_temp.filePath(QStringLiteral("state.json")),
                   QStringLiteral("--stop-file"), m_temp.filePath(QStringLiteral("stop"))});
  m_poll.start();
  m_timeout.start(30000);
  SetStatus(tr("Starting PartyPad…"));
}

void PartyPadProcess::Stop()
{
  if (!IsRunning())
    return;
  m_stopping = true;
  m_timeout.stop();
  m_poll.stop();
  m_url.clear();
  m_players.clear();
  QFile stop(m_temp.filePath(QStringLiteral("stop")));
  stop.open(QIODevice::WriteOnly);
  stop.close();
  m_kill_timer.start(5000);
  SetStatus(tr("PartyPad stopped. Select Retry to connect again."));
}

void PartyPadProcess::Poll()
{
  QFile file(m_temp.filePath(QStringLiteral("state.json")));
  if (!file.open(QIODevice::ReadOnly) || file.size() > 16384)
    return;
  const auto state = QJsonDocument::fromJson(file.readAll()).object();
  const QUrl url(state.value(QStringLiteral("join_url")).toString());
  if (state.value(QStringLiteral("version")).toInt() != 1 ||
      !state.value(QStringLiteral("ready")).toBool() || url.scheme() != QStringLiteral("https") ||
      url.host().isEmpty())
    return;
  m_timeout.stop();
  m_url = url.toString();
  m_players.clear();
  for (const auto& player : state.value(QStringLiteral("players")).toArray())
    m_players.append(player.toObject().value(QStringLiteral("player")).toInt());
  SetStatus(tr("Scan the QR code with phones on the same Wi-Fi network."));
}
