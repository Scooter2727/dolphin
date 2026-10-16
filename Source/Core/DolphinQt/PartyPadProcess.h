// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>

class PartyPadProcess final : public QObject
{
  Q_OBJECT
public:
  explicit PartyPadProcess(QObject* parent);
  ~PartyPadProcess() override;
  void Start(const QString& directory);
  void Stop();
  bool IsRunning() const;
  QString Status() const { return m_status; }
  QString JoinUrl() const { return m_url; }
  QString QrPath() const;
  bool IsPlayerConnected(int player) const;

signals:
  void Changed();

private:
  void Poll();
  void SetStatus(const QString& status);
  QProcess m_process;
  QTemporaryDir m_temp;
  QTimer m_poll;
  QTimer m_timeout;
  QTimer m_kill_timer;
  QString m_status;
  QString m_url;
  QString m_output;
  QList<int> m_players;
  bool m_stopping = false;
};
