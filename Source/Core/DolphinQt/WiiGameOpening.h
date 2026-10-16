// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QPixmap>
#include <QTimer>
#include <QWidget>
#include <memory>
#include "DolphinQt/WiiBanner.h"

// Full-screen channel preview. It never boots or mutates an emulated Wii.
class WiiGameOpening final : public QWidget
{
  Q_OBJECT
public:
  explicit WiiGameOpening(QWidget* parent = nullptr);
  ~WiiGameOpening() override;
  void Open(const QString& path, const QString& title, const QPixmap& cover);
  void Stop();
  void SetLaunchEnabled(bool enabled);
signals:
  void PlayRequested();
  void BackRequested();

protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void hideEvent(QHideEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;

private:
  void LayoutButtons();
  void StartSound();
  struct Sound;
  std::unique_ptr<Sound> m_sound;
  WiiBanner::Scene m_scene;
  QElapsedTimer m_elapsed;
  QTimer m_timer;
  class QPushButton* m_play = nullptr;
  class QPushButton* m_back = nullptr;
  QString m_title;
  QPixmap m_cover;
  QPixmap m_background;
  int m_generation = 0;
  bool m_loading = false;
};
