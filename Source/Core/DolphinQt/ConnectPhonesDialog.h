// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <array>

class QLabel;
class QLineEdit;
class QPushButton;
class PartyPadProcess;

class ConnectPhonesDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit ConnectPhonesDialog(QWidget* parent);
  void Start();

private:
  void Refresh();
  PartyPadProcess* m_server;
  QLineEdit* m_directory;
  QLineEdit* m_url;
  QLabel* m_status;
  QLabel* m_qr;
  QPushButton* m_retry;
  QPushButton* m_stop;
  QPushButton* m_browse;
  QPushButton* m_copy;
  std::array<QLabel*, 4> m_players;
};
