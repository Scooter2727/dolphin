// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QCursor>
#include <QElapsedTimer>
#include <QPixmap>
#include <QRectF>
#include <QString>
#include <QTimer>
#include <QTransform>
#include <QVariantAnimation>
#include <QWidget>
#include <array>
#include <vector>

class QAbstractButton;
class QPainter;
class GameListModel;
class PartyPadProcess;
class WiiGameOpening;

// Native counterpart of WiiTogetherDashboardPreview.html, using the existing
// Dolphin game model and the PartyPad process owned by ConnectPhonesDialog.
class WiiTogetherDashboard final : public QWidget
{
  Q_OBJECT
public:
  explicit WiiTogetherDashboard(QWidget* parent = nullptr);
  void SetGameModel(const GameListModel* model);
  void SetServer(PartyPadProcess* server);
  QString SelectedPath() const;
  void SetLaunchEnabled(bool enabled);

signals:
  void StartGameRequested();
  void ConnectPhonesRequested();
  void SettingsRequested();
  void LibraryRequested();
  void QuitRequested();

protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Game
  {
    QString path;
    QString title;
    QPixmap cover;
    bool banner_only = false;
  };
  void RefreshGames();
  void Browse(int direction);
  void SelectGame(int index, int direction = 0);
  void LayoutControls();
  void LayoutCoverControls();
  QTransform CoverTransform(int index, double offset) const;
  void UpdateFrame();
  void UpdatePlayerPanel();
  void OpenSettings();
  void OpenSelectedGame();
  void DrawCover(QPainter&, int index, double offset);
  void DrawPlayer(QPainter&, int player, bool hovered);
  void DrawJoinPanel(QPainter&);

  const GameListModel* m_model = nullptr;
  PartyPadProcess* m_server = nullptr;
  std::vector<Game> m_games;
  int m_selected = 0;
  double m_position = 0;
  bool m_launch_enabled = true;
  bool m_motion_enabled = true;
  bool m_pointer_enabled = true;
  int m_hover_player = -1;
  int m_pinned_player = -1;
  QElapsedTimer m_elapsed;
  QTimer m_frame_timer;
  QVariantAnimation m_slide;
  QRectF m_stage;
  QRectF m_cover_rect;
  double m_spread = 0;
  QPixmap m_background;
  QPixmap m_qr;
  QString m_qr_url;
  int m_clock_minute = -1;
  QCursor m_pointer;
  QAbstractButton* m_start_button = nullptr;
  QAbstractButton* m_left_button = nullptr;
  QAbstractButton* m_right_button = nullptr;
  QAbstractButton* m_settings_button = nullptr;
  QAbstractButton* m_quit_button = nullptr;
  QAbstractButton* m_library_button = nullptr;
  QAbstractButton* m_phone_button = nullptr;
  std::array<QAbstractButton*, 4> m_player_buttons{};
  std::vector<QAbstractButton*> m_cover_buttons;
  std::vector<QAbstractButton*> m_dots;
  QWidget* m_join_panel = nullptr;
  QAbstractButton* m_join_action = nullptr;
  WiiGameOpening* m_opening = nullptr;
  QString m_opening_path;
};
