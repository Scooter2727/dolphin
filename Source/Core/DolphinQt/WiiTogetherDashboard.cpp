// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "DolphinQt/WiiTogetherDashboard.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSettings>
#include <QShowEvent>
#include <QSvgRenderer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>

#include "DolphinQt/GameList/GameListModel.h"
#include "DolphinQt/PartyPadProcess.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/WiiGameOpening.h"
#include "UICommon/GameFile.h"

namespace
{
const QColor CYAN(QStringLiteral("#32b8e7"));
const std::array<QColor, 4> PLAYER_COLORS = {
    QColor(QStringLiteral("#27a9e3")), QColor(QStringLiteral("#ec6370")),
    QColor(QStringLiteral("#70b65b")), QColor(QStringLiteral("#e3ad33"))};
QColor Alpha(QColor color, double alpha)
{
  color.setAlphaF(qBound(0.0, alpha, 1.0));
  return color;
}
QFont PixelFont(const QFont& base, int size)
{
  QFont result(base);
  result.setPixelSize(size);
  return result;
}

// Real Qt buttons provide accessibility, tab focus, and keyboard activation;
// their appearance is drawn from the approved browser design.
class DashboardButton final : public QAbstractButton
{
public:
  using Paint = std::function<void(QPainter&, DashboardButton&)>;
  DashboardButton(QWidget* parent, const QString& name, Paint paint)
      : QAbstractButton(parent), m_paint(std::move(paint))
  {
    setText(name);
    setAccessibleName(name);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    m_paint(p, *this);
  }

private:
  Paint m_paint;
};

void DrawPill(QPainter& p, QRectF body, bool hovered, bool down, bool enabled, bool primary)
{
  body.adjust(4, 4, -4, -6);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(110, 130, 145, 28));
  p.drawRoundedRect(body.translated(0, 3), body.height() / 2, body.height() / 2);
  QLinearGradient gradient(body.topLeft(), body.bottomLeft());
  gradient.setColorAt(0, Qt::white);
  gradient.setColorAt(.4, Qt::white);
  gradient.setColorAt(.85, QColor(QStringLiteral("#f0f2f3")));
  gradient.setColorAt(1, QColor(down ? QStringLiteral("#cfe9f3") : QStringLiteral("#d9dfe2")));
  p.setBrush(gradient);
  p.setPen(QPen(enabled && (hovered || primary) ? CYAN : QColor(QStringLiteral("#c0c6ca")), 2));
  p.drawRoundedRect(body, body.height() / 2, body.height() / 2);
  p.setPen(QPen(QColor(255, 255, 255, 180), 2));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(body.adjusted(3, 3, -3, -3), body.height() / 2 - 3, body.height() / 2 - 3);
}

QAbstractButton* Pill(QWidget* parent, const QString& text, int size, bool primary = false)
{
  return new DashboardButton(parent, text, [size, primary](QPainter& p, DashboardButton& b) {
    DrawPill(p, b.rect(), b.underMouse() || b.hasFocus(), b.isDown(), b.isEnabled(), primary);
    p.setFont(PixelFont(b.font(), size));
    p.setPen(b.isEnabled() ? QColor(QStringLiteral("#69767e")) : QColor(QStringLiteral("#a8b0b6")));
    p.drawText(b.rect().adjusted(6, 0, -6, -2), Qt::AlignCenter, b.text());
  });
}

void DrawRemote(QPainter& p, const QRectF& rect)
{
  p.save();
  p.translate(rect.topLeft());
  p.scale(rect.width() / 28, rect.height() / 56);
  QPen pen = p.pen();
  pen.setWidthF(1.6);
  pen.setCapStyle(Qt::RoundCap);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(QRectF(6, 2, 16, 51), 5, 5);
  p.drawLine(QPointF(11, 12), QPointF(17, 12));
  p.drawLine(QPointF(14, 9), QPointF(14, 15));
  p.drawEllipse(QPointF(14, 23), 3, 3);
  for (const int y : {34, 42, 46})
    p.drawLine(QPointF(11, y), QPointF(17, y));
  p.restore();
}

class PaintedPanel final : public QWidget
{
public:
  PaintedPanel(QWidget* parent, std::function<void(QPainter&)> paint)
      : QWidget(parent), m_paint(std::move(paint))
  {
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    m_paint(p);
  }

private:
  std::function<void(QPainter&)> m_paint;
};

class DashboardDialog final : public QDialog
{
public:
  explicit DashboardDialog(QWidget* parent) : QDialog(parent) {}

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0, Qt::white);
    gradient.setColorAt(1, QColor(QStringLiteral("#eef3f6")));
    painter.setBrush(gradient);
    painter.setPen(QPen(QColor(QStringLiteral("#4ec2e9")), 3));
    painter.drawRoundedRect(QRectF(rect()).adjusted(2, 2, -2, -2), 26, 26);
  }
};
}  // namespace

WiiTogetherDashboard::WiiTogetherDashboard(QWidget* parent) : QWidget(parent)
{
  setMinimumSize(760, 580);
  setAttribute(Qt::WA_OpaquePaintEvent);
  setFocusPolicy(Qt::StrongFocus);
  const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/wiitogether/font.ttf"));
  const auto families = QFontDatabase::applicationFontFamilies(id);
  if (!families.isEmpty())
    setFont(PixelFont(QFont(families.front()), 16));
  m_background.load(QStringLiteral(":/wiitogether/background.png"));
  m_pointer = QCursor(QPixmap(QStringLiteral(":/wiitogether/pointer.png"))
                          .scaled(48, 52, Qt::KeepAspectRatio, Qt::SmoothTransformation),
                      19, 3);
  auto& settings = Settings::GetQSettings();
  m_motion_enabled = settings.value(QStringLiteral("wiitogether/animations"), true).toBool();
  m_pointer_enabled = settings.value(QStringLiteral("wiitogether/pointer"), true).toBool();
  setCursor(m_pointer_enabled ? m_pointer : QCursor(Qt::ArrowCursor));

  const auto system_button = [this](const QString& text, bool power) {
    return new DashboardButton(this, text, [power](QPainter& p, DashboardButton& b) {
      const bool hovered = b.underMouse() || b.hasFocus();
      DrawPill(p, b.rect(), hovered, b.isDown(), true, false);
      p.setPen(QPen(hovered ? CYAN : QColor(QStringLiteral("#9da5ab")), 1.8));
      p.setBrush(Qt::NoBrush);
      p.save();
      p.translate(b.width() / 2.0 - (power ? 34 : 48), b.height() / 2.0 - 12);
      if (power)
      {
        p.drawArc(QRectF(3, 4, 18, 18), 130 * 16, 280 * 16);
        p.drawLine(QPointF(12, 1), QPointF(12, 12));
      }
      else
      {
        QPolygonF gear;
        for (int i = 0; i < 32; ++i)
        {
          const double angle = i * 2 * 3.141592653589793 / 32;
          const double radius = i % 4 < 2 ? 10 : 7.7;
          gear << QPointF(12 + radius * std::cos(angle), 12 + radius * std::sin(angle));
        }
        p.drawPolygon(gear);
        p.drawEllipse(QPointF(12, 12), 3.5, 3.5);
      }
      p.restore();
      p.setFont(PixelFont(b.font(), b.width() < 130 ? 15 : 18));
      p.setPen(QColor(QStringLiteral("#69767e")));
      p.drawText(b.rect().adjusted(24, 0, 0, -2), Qt::AlignCenter, b.text());
    });
  };
  m_quit_button = system_button(tr("Quit"), true);
  m_settings_button = system_button(tr("Settings"), false);
  m_start_button = Pill(this, tr("Start Game"), 22, true);
  m_library_button =
      new DashboardButton(this, tr("Game Library"), [](QPainter& p, DashboardButton& b) {
        p.setFont(PixelFont(b.font(), 12));
        p.setPen(b.underMouse() || b.hasFocus() ? CYAN : QColor(QStringLiteral("#8f99a2")));
        p.drawText(b.rect(), Qt::AlignRight | Qt::AlignVCenter, b.text());
      });
  m_phone_button =
      new DashboardButton(this, tr("Connect Phones"), [this](QPainter& p, DashboardButton& b) {
        p.setFont(PixelFont(b.font(), 14));
        const bool ready = m_server && !m_server->JoinUrl().isEmpty();
        int count = 0;
        for (int i = 1; i <= 4; ++i)
          count += m_server && m_server->IsPlayerConnected(i);
        p.setPen(Qt::NoPen);
        p.setBrush(ready ? CYAN : QColor(QStringLiteral("#afb6bc")));
        p.drawEllipse(QPointF(6, b.height() / 2.0), 3.5, 3.5);
        p.setPen(b.underMouse() || b.hasFocus() ? CYAN : QColor(QStringLiteral("#7f8a91")));
        const QString status = count ? tr("%n player(s) ready to play", nullptr, count) :
                               ready ? tr("Scan a code to play together") :
                                       tr("Connect a phone to play together");
        p.drawText(b.rect().adjusted(19, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, status);
      });
  const auto arrow = [this](bool left) {
    return new DashboardButton(
        this, left ? tr("Previous game") : tr("Next game"),
        [left](QPainter& p, DashboardButton& b) {
          const bool hover = b.underMouse() || b.hasFocus();
          const QString file = left ? (hover ? QStringLiteral(":/wiitogether/backhover.png") :
                                               QStringLiteral(":/wiitogether/back.png")) :
                                      (hover ? QStringLiteral(":/wiitogether/forwardhover.png") :
                                               QStringLiteral(":/wiitogether/forward.png"));
          const QPixmap image(file);
          p.setOpacity(b.isEnabled() ? 1 : .25);
          const QSize size = image.size().scaled(b.size() - QSize(hover ? 0 : 6, hover ? 0 : 6),
                                                 Qt::KeepAspectRatio);
          p.drawPixmap(
              QRect(QPoint((b.width() - size.width()) / 2, (b.height() - size.height()) / 2), size),
              image);
        });
  };
  m_left_button = arrow(true);
  m_right_button = arrow(false);
  for (int i = 0; i < 4; ++i)
  {
    m_player_buttons[i] = new DashboardButton(
        this, tr("Connect player %1").arg(i + 1), [this, i](QPainter& p, DashboardButton& b) {
          DrawPlayer(p, i, b.underMouse() || b.hasFocus() || m_hover_player == i);
        });
    connect(m_player_buttons[i], &QAbstractButton::clicked, this, [this, i] {
      m_pinned_player = m_pinned_player == i ? -1 : i;
      UpdatePlayerPanel();
    });
  }
  m_join_panel = new PaintedPanel(this, [this](QPainter& p) { DrawJoinPanel(p); });
  m_join_panel->setAccessibleName(tr("Phone connection"));
  auto* shadow = new QGraphicsDropShadowEffect(m_join_panel);
  shadow->setBlurRadius(25);
  shadow->setOffset(0, 6);
  shadow->setColor(QColor(82, 105, 126, 54));
  m_join_panel->setGraphicsEffect(shadow);
  m_join_action = Pill(m_join_panel, tr("Connect Phones"), 13);
  m_join_panel->hide();
  connect(m_join_action, &QAbstractButton::clicked, this,
          &WiiTogetherDashboard::ConnectPhonesRequested);
  connect(m_phone_button, &QAbstractButton::clicked, this,
          &WiiTogetherDashboard::ConnectPhonesRequested);
  connect(m_quit_button, &QAbstractButton::clicked, this, &WiiTogetherDashboard::QuitRequested);
  connect(m_settings_button, &QAbstractButton::clicked, this, &WiiTogetherDashboard::OpenSettings);
  connect(m_library_button, &QAbstractButton::clicked, this,
          &WiiTogetherDashboard::LibraryRequested);
  connect(m_start_button, &QAbstractButton::clicked, this, [this] { OpenSelectedGame(); });
  connect(m_left_button, &QAbstractButton::clicked, this, [this] { Browse(-1); });
  connect(m_right_button, &QAbstractButton::clicked, this, [this] { Browse(1); });
  for (auto* button : findChildren<QAbstractButton*>())
    button->installEventFilter(this);
  m_opening = new WiiGameOpening(this);
  m_opening->hide();
  connect(m_opening, &WiiGameOpening::BackRequested, this, [this] {
    m_opening->hide();
    m_opening_path.clear();
    m_frame_timer.start(m_motion_enabled ? 33 : 100);
    setFocus();
    update();
  });
  connect(m_opening, &WiiGameOpening::PlayRequested, this, [this] {
    m_opening->hide();
    m_frame_timer.start(m_motion_enabled ? 33 : 100);
    if (m_launch_enabled && !SelectedPath().isEmpty())
      emit StartGameRequested();
    m_opening_path.clear();
  });
  m_slide.setDuration(460);
  m_slide.setEasingCurve(QEasingCurve::OutCubic);
  connect(&m_slide, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
    m_position = value.toDouble();
    LayoutCoverControls();
    update();
  });
  connect(&m_slide, &QVariantAnimation::finished, this, [this] {
    m_position = m_selected;
    LayoutControls();
    update();
  });
  m_elapsed.start();
  m_frame_timer.setInterval(33);
  connect(&m_frame_timer, &QTimer::timeout, this, &WiiTogetherDashboard::UpdateFrame);
  LayoutControls();
}

void WiiTogetherDashboard::SetGameModel(const GameListModel* model)
{
  if (m_model)
    disconnect(m_model, nullptr, this, nullptr);
  m_model = model;
  connect(model, &QAbstractItemModel::rowsInserted, this, &WiiTogetherDashboard::RefreshGames);
  connect(model, &QAbstractItemModel::rowsRemoved, this, &WiiTogetherDashboard::RefreshGames);
  connect(model, &QAbstractItemModel::modelReset, this, &WiiTogetherDashboard::RefreshGames);
  connect(model, &QAbstractItemModel::dataChanged, this, &WiiTogetherDashboard::RefreshGames);
  connect(model, &QAbstractItemModel::layoutChanged, this, &WiiTogetherDashboard::RefreshGames);
  RefreshGames();
}

void WiiTogetherDashboard::RefreshGames()
{
  const QString previous = SelectedPath();
  m_slide.stop();
  m_games.clear();
  for (auto* button : m_cover_buttons)
    delete button;
  for (auto* dot : m_dots)
    delete dot;
  m_cover_buttons.clear();
  m_dots.clear();
  if (m_model)
  {
    for (int row = 0; row < m_model->rowCount({}); ++row)
    {
      if (!m_model->ShouldDisplayGameListItem(row))
        continue;
      const auto file = m_model->GetGameFile(row);
      Game game;
      game.path = QString::fromStdString(file->GetFilePath());
      game.title =
          m_model->data(m_model->index(row, static_cast<int>(GameListModel::Column::Title)))
              .toString();
      const auto& buffer = file->GetCoverImage().buffer;
      if (!buffer.empty())
        game.cover.loadFromData(buffer.data(), static_cast<uint>(buffer.size()));
      // Bundled artwork is used only for a matching game in the real library.
      if (game.cover.isNull())
        game.cover.load(QStringLiteral(":/wiitogether/covers/%1.png")
                            .arg(QString::fromStdString(file->GetGameID())));
      if (game.cover.isNull())
      {
        game.cover =
            m_model
                ->data(m_model->index(row, static_cast<int>(GameListModel::Column::Banner)),
                       Qt::DecorationRole)
                .value<QPixmap>();
        game.banner_only = true;
      }
      m_games.push_back(std::move(game));
    }
  }
  std::stable_sort(m_games.begin(), m_games.end(), [](const Game& a, const Game& b) {
    return QString::localeAwareCompare(a.title, b.title) < 0;
  });
  m_selected = 0;
  for (int i = 0; i < static_cast<int>(m_games.size()); ++i)
  {
    if (m_games[i].path == previous)
      m_selected = i;
    auto* cover = new DashboardButton(this, m_games[i].title, [](QPainter&, DashboardButton&) {});
    cover->setToolTip(m_games[i].title);
    connect(cover, &QAbstractButton::clicked, this, [this, i] {
      SelectGame(i);
      OpenSelectedGame();
    });
    cover->installEventFilter(this);
    m_cover_buttons.push_back(cover);
    auto* dot = new DashboardButton(
        this, tr("Select %1").arg(m_games[i].title), [this, i](QPainter& p, DashboardButton& b) {
          p.setPen(Qt::NoPen);
          p.setBrush(i == m_selected ? CYAN : QColor(QStringLiteral("#c9cfd2")));
          p.drawEllipse(QPointF(b.width() / 2.0, b.height() / 2.0), b.hasFocus() ? 5 : 4,
                        b.hasFocus() ? 5 : 4);
        });
    connect(dot, &QAbstractButton::clicked, this, [this, i] { SelectGame(i); });
    dot->installEventFilter(this);
    m_dots.push_back(dot);
  }
  m_position = m_selected;
  SetLaunchEnabled(m_launch_enabled);
  m_left_button->setEnabled(m_games.size() > 1);
  m_right_button->setEnabled(m_games.size() > 1);
  LayoutControls();
  update();
}

void WiiTogetherDashboard::SetServer(PartyPadProcess* server)
{
  if (m_server)
    disconnect(m_server, nullptr, this, nullptr);
  m_server = server;
  connect(server, &PartyPadProcess::Changed, this, [this] {
    if (m_server->JoinUrl() != m_qr_url)
    {
      m_qr_url = m_server->JoinUrl();
      m_qr = QPixmap();
      if (!m_qr_url.isEmpty())
      {
        QSvgRenderer qr(m_server->QrPath());
        m_qr = QPixmap(384, 384);
        m_qr.fill(Qt::white);
        QPainter painter(&m_qr);
        qr.render(&painter);
      }
    }
    m_phone_button->setToolTip(m_server->Status());
    for (int i = 0; i < 4; ++i)
    {
      m_player_buttons[i]->setAccessibleDescription(
          m_server->IsPlayerConnected(i + 1) ? tr("Connected") : tr("Waiting for a phone"));
      m_player_buttons[i]->update();
    }
    m_phone_button->update();
    m_join_panel->update();
  });
}

QString WiiTogetherDashboard::SelectedPath() const
{
  if (!m_opening_path.isEmpty())
    return m_opening_path;
  return m_games.empty() ? QString{} : m_games[m_selected].path;
}
void WiiTogetherDashboard::SetLaunchEnabled(bool enabled)
{
  m_launch_enabled = enabled;
  m_start_button->setEnabled(enabled && !m_games.empty());
  if (m_opening)
    m_opening->SetLaunchEnabled(enabled);
}

void WiiTogetherDashboard::OpenSelectedGame()
{
  if (m_games.empty() || !m_launch_enabled)
    return;
  m_slide.stop();
  m_position = m_selected;
  m_frame_timer.stop();
  m_pinned_player = -1;
  m_hover_player = -1;
  m_join_panel->hide();
  m_opening->setGeometry(rect());
  m_opening->show();
  m_opening->raise();
  const auto& game = m_games[m_selected];
  m_opening_path = game.path;
  m_opening->Open(game.path, game.title, game.cover);
}
void WiiTogetherDashboard::Browse(int direction)
{
  const int count = static_cast<int>(m_games.size());
  if (count > 1)
    SelectGame((m_selected + direction + count) % count, direction);
}
void WiiTogetherDashboard::SelectGame(int index, int direction)
{
  if (index < 0 || index >= static_cast<int>(m_games.size()))
    return;
  m_slide.stop();
  double delta = std::remainder(index - m_position, static_cast<double>(m_games.size()));
  if (direction && std::abs(delta) == m_games.size() / 2.0)
    delta = std::abs(delta) * direction;
  m_selected = index;
  if (m_motion_enabled)
  {
    m_slide.setStartValue(m_position);
    m_slide.setEndValue(m_position + delta);
    m_slide.start();
  }
  else
    m_position = index;
  for (auto* dot : m_dots)
    dot->update();
  LayoutControls();
  update();
}

void WiiTogetherDashboard::LayoutControls()
{
  if (m_opening && m_opening->isVisible())
    m_opening->setGeometry(rect());
  const double w = width(), h = height();
  const bool short_window = h <= 710, narrow = w <= 850;
  const int header_height = short_window ? 135 : h >= 850 ? 160 : 150;
  const int lobby_height = short_window ? 160 : h >= 850 ? 190 : 178;
  const int selection_height = short_window ? 91 : 115;
  const double available = h - lobby_height - header_height - selection_height;
  const double stage_height = qMin(available, h >= 850 ? 450.0 : 390.0);
  m_stage = QRectF(0, header_height + qMax(0.0, (available - stage_height) / 2), w, stage_height);
  const double ch = qMin(stage_height - (h >= 850 ? 45 : 32), h >= 850 ? 370.0 : 325.0);
  m_cover_rect = QRectF(-ch * 160 / 225 / 2, -ch / 2, ch * 160 / 225, ch);
  m_spread = narrow ? w * .19 : qBound(145.0, w * .185, 260.0);
  const int sw = narrow ? 112 : 146;
  m_quit_button->setGeometry(qRound(w * .046) - 4, short_window ? 64 : 71, sw, 55);
  m_settings_button->setGeometry(qRound(w * .954) - sw + 4, short_window ? 64 : 71, sw, 55);
  m_left_button->setGeometry(qRound(w * .044), qRound(m_stage.center().y()) - 31, 44, 62);
  m_right_button->setGeometry(qRound(w * .956) - 44, qRound(m_stage.center().y()) - 31, 44, 62);
  const int sh = short_window ? 53 : 65;
  const int sy = qRound(h - lobby_height - (short_window ? 9 : 16) - sh);
  m_start_button->setGeometry(qRound(w / 2) - 124, sy, 248, sh);
  // Bound the pagination group for large real libraries.
  const int dot_count = qMin(9, static_cast<int>(m_dots.size()));
  const int first =
      qBound(0, m_selected - dot_count / 2, static_cast<int>(m_dots.size()) - dot_count);
  for (int i = 0; i < static_cast<int>(m_dots.size()); ++i)
  {
    const bool visible = i >= first && i < first + dot_count;
    m_dots[i]->setVisible(visible);
    if (visible)
      m_dots[i]->setGeometry(qRound(w / 2 - dot_count * 9 + (i - first) * 18), sy - 30, 18, 20);
  }
  const int margin = qRound(w * (narrow ? .03 : .045));
  const int gap = narrow ? 8 : h >= 850 ? 18 : 13;
  const double card_width = (w - 2 * margin - 3 * gap) / 4;
  const int card_height = short_window ? 73 : h >= 850 ? 90 : 82;
  const int card_y = qRound(h) - 43 - card_height;
  for (int i = 0; i < 4; ++i)
    m_player_buttons[i]->setGeometry(qRound(margin + i * (card_width + gap)), card_y,
                                     qRound(card_width), card_height + 6);
  m_phone_button->setGeometry(margin + 8, card_y - 31, qRound(w * .65), 25);
  m_library_button->setGeometry(qRound(w) - margin - 125, qRound(h) - 29, 125, 22);
  LayoutCoverControls();
  UpdatePlayerPanel();
  if (m_opening && m_opening->isVisible())
    m_opening->raise();
}

QTransform WiiTogetherDashboard::CoverTransform(int index, double offset) const
{
  const double distance = std::abs(offset);
  const double scale = distance <= 1 ? 1 - distance * .18 : .82 - (distance - 1) * .15;
  const double seconds = m_elapsed.elapsed() / 1000.0;
  const double float_y = m_motion_enabled ? -2 + 2 * std::cos(seconds * 1.047 + index * .733) : 0;
  QTransform transform;
  transform.translate(m_stage.center().x() + offset * m_spread, m_stage.center().y() + float_y);
  transform.rotate(qBound(-25.0, -offset * 25, 25.0), Qt::YAxis);
  transform.scale(scale, scale);
  return transform;
}

void WiiTogetherDashboard::LayoutCoverControls()
{
  std::vector<std::pair<double, int>> visible;
  for (int i = 0; i < static_cast<int>(m_games.size()); ++i)
  {
    const double offset = std::remainder(i - m_position, static_cast<double>(m_games.size()));
    const bool shown = std::abs(offset) <= 2.65;
    m_cover_buttons[i]->setVisible(shown);
    if (shown)
    {
      const QRect geometry = CoverTransform(i, offset).mapRect(m_cover_rect).toAlignedRect();
      if (m_cover_buttons[i]->geometry() != geometry)
        m_cover_buttons[i]->setGeometry(geometry);
      visible.emplace_back(std::abs(offset), i);
    }
  }
  std::sort(visible.rbegin(), visible.rend());
  for (const auto& [distance, index] : visible)
    m_cover_buttons[index]->raise();
  m_left_button->raise();
  m_right_button->raise();
  if (m_join_panel->isVisible())
    m_join_panel->raise();
}

void WiiTogetherDashboard::UpdateFrame()
{
  const int minute = QTime::currentTime().minute();
  if (minute != m_clock_minute)
  {
    m_clock_minute = minute;
    update();
  }
  const QPoint cursor = mapFromGlobal(QCursor::pos());
  int hovered = -1;
  for (int i = 0; i < 4; ++i)
    if (m_player_buttons[i]->geometry().contains(cursor) || m_player_buttons[i]->hasFocus())
      hovered = i;
  if (hovered < 0 && m_join_panel->isVisible() &&
      (m_join_panel->geometry().adjusted(0, 0, 0, 20).contains(cursor) ||
       m_join_action->hasFocus()))
    hovered = m_hover_player;
  if (hovered != m_hover_player)
  {
    m_hover_player = hovered;
    UpdatePlayerPanel();
  }
  if (m_motion_enabled || m_slide.state() == QAbstractAnimation::Running)
  {
    LayoutCoverControls();
    update();
    for (auto* player : m_player_buttons)
      player->update();
  }
}

void WiiTogetherDashboard::UpdatePlayerPanel()
{
  const int player = m_pinned_player >= 0 ? m_pinned_player : m_hover_player;
  if (player < 0)
  {
    m_join_panel->hide();
    return;
  }
  const int pw = width() <= 850 ? 205 : 240;
  const int ph = height() <= 710 ? 296 : 322;
  const QRect card = m_player_buttons[player]->geometry();
  const int x = qBound(8, card.center().x() - pw / 2, width() - pw - 8);
  m_join_panel->setGeometry(x, card.top() - ph - 7, pw, ph);
  m_join_action->setGeometry(22, ph - 69, pw - 44, 40);
  m_join_action->setText(m_server && !m_server->JoinUrl().isEmpty() ? tr("Connection options") :
                                                                      tr("Connect Phones"));
  m_join_panel->show();
  m_join_panel->raise();
  m_join_panel->update();
  for (auto* b : m_player_buttons)
    b->update();
}

void WiiTogetherDashboard::DrawPlayer(QPainter& p, int player, bool hovered)
{
  const bool active = m_server && m_server->IsPlayerConnected(player + 1);
  const bool narrow = width() <= 850;
  const QRectF body = QRectF(m_player_buttons[player]->rect()).adjusted(1, 1, -1, -6);
  const QColor color = active ? PLAYER_COLORS[player] : QColor(QStringLiteral("#a4acb3"));
  QLinearGradient gradient(body.topLeft(), body.bottomLeft());
  gradient.setColorAt(0, active ? Qt::white : QColor(QStringLiteral("#f7f8f9")));
  gradient.setColorAt(.45, active ? Alpha(color, .10) : QColor(QStringLiteral("#eceef1")));
  gradient.setColorAt(1, active ? Alpha(color, .28) : QColor(QStringLiteral("#dce0e5")));
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(129, 146, 163, 35));
  p.drawRoundedRect(body.translated(0, 3), 15, 15);
  p.setBrush(Qt::white);
  p.drawRoundedRect(body, 15, 15);
  p.setBrush(gradient);
  p.setPen(QPen(hovered ? CYAN : active ? color : QColor(QStringLiteral("#b4bbc3")), 2));
  p.drawRoundedRect(body, 15, 15);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(255, 255, 255, 185), 2));
  p.drawRoundedRect(body.adjusted(3, 3, -3, -3), 12, 12);
  const double glow = m_motion_enabled ? .8 + .2 * std::sin(m_elapsed.elapsed() / 605.0) : 1;
  p.setPen(QPen(active ? Alpha(color, glow) : QColor(QStringLiteral("#c5cbd1")), 3, Qt::SolidLine,
                Qt::RoundCap));
  p.drawLine(QPointF(20, body.bottom() - 7), QPointF(body.right() - 20, body.bottom() - 7));
  p.setPen(hovered ? CYAN : color);
  DrawRemote(p, QRectF(narrow ? 9 : 15, body.center().y() - 24, 24, 47));
  const int tx = narrow ? 39 : 52;
  p.setFont(PixelFont(font(), narrow ? 16 : 20));
  p.drawText(QRectF(tx, body.center().y() - 20, body.width() - tx, 25), Qt::AlignVCenter,
             tr("Player %1").arg(player + 1));
  p.setFont(PixelFont(font(), narrow ? 10 : 12));
  p.setPen(active ? QColor(QStringLiteral("#7b8b96")) : QColor(QStringLiteral("#a5adb4")));
  p.drawText(QRectF(tx, body.center().y() + 5, body.width() - tx, 17), Qt::AlignVCenter,
             active ? tr("Ready to play") : tr("Connect a phone"));
  if (!narrow && body.width() > 210)
  {
    const QPointF c(body.right() - 27, body.center().y());
    p.setPen(QPen(active ? color : QColor(QStringLiteral("#c0c9d0")), 1));
    p.setBrush(QColor(255, 255, 255, 100));
    p.drawEllipse(c, 13, 13);
    p.setFont(PixelFont(font(), 15));
    p.drawText(QRectF(c.x() - 13, c.y() - 13, 26, 26), Qt::AlignCenter,
               QString::number(player + 1));
  }
}

void WiiTogetherDashboard::DrawJoinPanel(QPainter& p)
{
  const QRectF body(1, 1, m_join_panel->width() - 2, m_join_panel->height() - 13);
  QLinearGradient gradient(body.topLeft(), body.bottomLeft());
  gradient.setColorAt(0, Qt::white);
  gradient.setColorAt(1, QColor(QStringLiteral("#f0f4f7")));
  p.setPen(QPen(QColor(QStringLiteral("#55c4eb")), 2));
  p.setBrush(gradient);
  p.drawRoundedRect(body, 20, 20);
  QPolygonF tail;
  tail << QPointF(body.center().x() - 9, body.bottom() - 1)
       << QPointF(body.center().x(), body.bottom() + 9)
       << QPointF(body.center().x() + 9, body.bottom() - 1);
  p.drawPolyline(tail);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(QStringLiteral("#f0f4f7")));
  p.drawPolygon(tail);
  p.setPen(QColor(QStringLiteral("#6b7c86")));
  p.setFont(PixelFont(font(), 19));
  p.drawText(QRectF(10, 17, body.width() - 20, 26), Qt::AlignCenter, tr("Let's play together."));
  p.setFont(PixelFont(font(), 12));
  p.setPen(QColor(QStringLiteral("#919ca4")));
  p.drawText(QRectF(10, 45, body.width() - 20, 20), Qt::AlignCenter,
             tr("Scan with your phone to join."));
  const int qs = height() <= 710 ? 130 : 152;
  const QRect qr_rect((m_join_panel->width() - qs) / 2, 74, qs, qs);
  p.fillRect(qr_rect, Qt::white);
  if (!m_qr.isNull())
    p.drawPixmap(qr_rect, m_qr);
  else
  {
    DrawRemote(p, QRectF(qr_rect.center().x() - 16, qr_rect.top() + 15, 32, 58));
    p.drawText(qr_rect.adjusted(8, 78, -8, -2), Qt::AlignCenter | Qt::TextWordWrap,
               tr("Start Connect Phones for a live QR code"));
  }
  p.drawText(QRectF(6, qr_rect.bottom() + 6, body.width() - 12, 22), Qt::AlignCenter,
             tr("Same Wi-Fi. No extra controllers."));
  p.setFont(PixelFont(font(), 10));
  p.drawText(QRectF(10, body.bottom() - 24, body.width() - 20, 18), Qt::AlignCenter,
             m_qr.isNull() ? tr("Live connection through PartyPad") :
                             tr("Open your phone camera to scan"));
}

void WiiTogetherDashboard::DrawCover(QPainter& p, int index, double offset)
{
  const double distance = std::abs(offset);
  const double scale = distance <= 1 ? 1 - distance * .18 : .82 - (distance - 1) * .15;
  const double seconds = m_elapsed.elapsed() / 1000.0;
  const double float_y = m_motion_enabled ? -2 + 2 * std::cos(seconds * 1.047 + index * .733) : 0;
  const QPointF center(m_stage.center().x() + offset * m_spread, m_stage.center().y() + float_y);
  p.save();
  p.setOpacity(qBound(0.0, 1 - distance * .16, 1.0));
  const double shadow_w = m_cover_rect.width() * scale * 1.28;
  p.save();
  p.translate(center.x(), m_stage.center().y() + m_cover_rect.height() * scale / 2 + 20);
  p.scale(1, 18 / shadow_w);
  QRadialGradient shadow(QPointF(0, 0), shadow_w / 2);
  shadow.setColorAt(0, QColor(92, 107, 125, 55));
  shadow.setColorAt(1, Qt::transparent);
  p.setPen(Qt::NoPen);
  p.setBrush(shadow);
  p.drawEllipse(QPointF(0, 0), shadow_w / 2, shadow_w / 2);
  p.restore();
  const QTransform transform = CoverTransform(index, offset);
  p.setTransform(transform, true);
  const QRectF r = m_cover_rect;
  if (index == m_selected)
  {
    const double pulse = m_motion_enabled ? .72 + .27 * std::sin(seconds * 1.571) : .7;
    p.setPen(QPen(Alpha(CYAN, pulse), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(-12, -12, 12, 12), 12, 12);
    p.setPen(QPen(QColor(QStringLiteral("#57c7eb")), 6));
    p.drawRoundedRect(r.adjusted(-3, -3, 3, 3), 7, 7);
    p.setPen(QPen(Qt::white, 3));
    p.drawRoundedRect(r.adjusted(-1, -1, 1, 1), 7, 7);
  }
  QPainterPath clip;
  clip.addRoundedRect(r, 6, 6);
  p.setClipPath(clip);
  p.fillRect(r, Qt::white);
  const auto& game = m_games[index];
  if (!game.cover.isNull() && !game.banner_only)
    p.drawPixmap(r, game.cover, game.cover.rect());
  else
  {
    QSvgRenderer logo(QStringLiteral(":/wiitogether/logo.svg"));
    logo.render(&p, QRectF(r.left() + 10, r.top() + 8, 40, 20));
    if (!game.cover.isNull())
    {
      const QSize size = game.cover.size().scaled(
          QSize(qRound(r.width()) - 16, qRound(r.height() * .5)), Qt::KeepAspectRatio);
      p.drawPixmap(QRectF(r.center() - QPointF(size.width() / 2.0, size.height() / 2.0), size),
                   game.cover, game.cover.rect());
    }
    p.setPen(QColor(QStringLiteral("#7f8a91")));
    p.setFont(PixelFont(font(), 14));
    p.drawText(r.adjusted(12, r.height() * .72, -12, -12), Qt::AlignHCenter | Qt::TextWordWrap,
               game.title);
  }
  QLinearGradient gloss(r.topLeft(), r.topRight());
  gloss.setColorAt(0, QColor(255, 255, 255, 80));
  gloss.setColorAt(.23, Qt::transparent);
  gloss.setColorAt(.75, Qt::transparent);
  gloss.setColorAt(1, QColor(0, 0, 0, 18));
  p.fillRect(r, gloss);
  p.setClipping(false);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(QStringLiteral("#c3c8cb")), 1));
  p.drawRoundedRect(r, 6, 6);
  p.restore();
  auto* button = m_cover_buttons[index];
  if (button->hasFocus())
  {
    p.setPen(QPen(CYAN, 2, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(button->geometry().adjusted(-5, -5, 5, 5), 9, 9);
  }
}

void WiiTogetherDashboard::paintEvent(QPaintEvent*)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.fillRect(rect(), QColor(QStringLiteral("#e9e9e9")));
  if (!m_background.isNull())
    p.drawTiledPixmap(rect(), m_background);
  p.save();
  p.translate(width() / 2.0, height() * .34);
  p.scale(1, height() / static_cast<double>(width()));
  QRadialGradient light(QPointF(0, 0), width() * .75);
  light.setColorAt(.05, Qt::white);
  light.setColorAt(.38, QColor(255, 255, 255, 176));
  light.setColorAt(1, Qt::transparent);
  p.fillRect(QRectF(-width(), -height(), width() * 2, height() * 3), light);
  p.restore();
  QSvgRenderer header(QStringLiteral(":/wiitogether/header.svg"));
  header.setAspectRatioMode(Qt::IgnoreAspectRatio);
  header.render(&p, QRectF(0, 0, width(), height() <= 710 ? 100 : 110));
  const bool narrow = width() <= 850;
  const int ly = height() <= 710 ? 12 : 16;
  const int lw = narrow ? 67 : height() <= 710 ? 72 : 82;
  const QFont title_font = PixelFont(font(), narrow ? 29 : height() <= 710 ? 31 : 35);
  const int tw = QFontMetrics(title_font).horizontalAdvance(tr("Together"));
  const double lx = (width() - lw - 11 - tw) / 2.0;
  p.setOpacity(.86);
  QSvgRenderer logo(QStringLiteral(":/wiitogether/logo.svg"));
  logo.render(&p, QRectF(lx, ly, lw, 39));
  p.setFont(title_font);
  p.setPen(QColor(QStringLiteral("#8c9194")));
  p.drawText(QRectF(lx + lw + 11, ly, tw + 3, 39), Qt::AlignVCenter, tr("Together"));
  p.setOpacity(1);
  p.setFont(PixelFont(font(), 18));
  p.setPen(QColor(QStringLiteral("#909ba2")));
  p.drawText(QRectF(width() - width() * .046 - 180, ly - 1, 180, 25), Qt::AlignRight,
             QLocale().toString(QTime::currentTime(), QLocale::ShortFormat));
  if (!m_games.empty())
  {
    std::vector<std::pair<double, int>> visible;
    for (int i = 0; i < static_cast<int>(m_games.size()); ++i)
    {
      const double offset = std::remainder(i - m_position, static_cast<double>(m_games.size()));
      const bool shown = std::abs(offset) <= 2.65;
      if (shown)
        visible.emplace_back(offset, i);
    }
    std::sort(visible.begin(), visible.end(),
              [](const auto& a, const auto& b) { return std::abs(a.first) > std::abs(b.first); });
    for (const auto& [offset, index] : visible)
      DrawCover(p, index, offset);
  }
  else
  {
    const QRectF empty(width() / 2.0 - 190, m_stage.center().y() - 72, 380, 144);
    p.setPen(QPen(QColor(QStringLiteral("#c6d3da")), 1));
    p.setBrush(QColor(255, 255, 255, 160));
    p.drawRoundedRect(empty, 20, 20);
    p.setFont(PixelFont(font(), 22));
    p.setPen(QColor(QStringLiteral("#697b86")));
    p.drawText(empty.adjusted(20, 18, -20, -75), Qt::AlignCenter,
               tr("Your games will appear here"));
    p.setFont(PixelFont(font(), 15));
    p.drawText(empty.adjusted(30, 70, -30, -15), Qt::AlignCenter | Qt::TextWordWrap,
               tr("Open Game Library below to add your Wii and GameCube games."));
  }
  p.setFont(PixelFont(font(), 12));
  p.setPen(QColor(QStringLiteral("#8f99a2")));
  p.drawText(QRect(160, height() - 29, width() - 320, 22), Qt::AlignCenter,
             tr("Enter: Select    |    Point at a player to connect"));
}

void WiiTogetherDashboard::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  LayoutControls();
}
void WiiTogetherDashboard::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  if (m_model)
    RefreshGames();
  m_frame_timer.start(m_motion_enabled ? 33 : 100);
  LayoutControls();
}
void WiiTogetherDashboard::hideEvent(QHideEvent* event)
{
  if (m_opening)
    m_opening->hide();
  m_opening_path.clear();
  m_frame_timer.stop();
  m_slide.stop();
  m_position = m_selected;
  QWidget::hideEvent(event);
}
bool WiiTogetherDashboard::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::KeyPress)
  {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Left || key->key() == Qt::Key_Right || key->key() == Qt::Key_Escape)
    {
      keyPressEvent(key);
      return true;
    }
  }
  if (event->type() == QEvent::Enter || event->type() == QEvent::Leave ||
      event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)
    update();
  return QWidget::eventFilter(object, event);
}
void WiiTogetherDashboard::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Left)
    Browse(-1);
  else if (event->key() == Qt::Key_Right)
    Browse(1);
  else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    m_start_button->click();
  else if (event->key() == Qt::Key_Escape)
  {
    m_pinned_player = -1;
    m_hover_player = -1;
    m_join_panel->hide();
    setFocus();
  }
  else
    QWidget::keyPressEvent(event);
}

void WiiTogetherDashboard::OpenSettings()
{
  m_pinned_player = -1;
  m_hover_player = -1;
  m_join_panel->hide();
  PaintedPanel scrim(
      this, [this](QPainter& painter) { painter.fillRect(rect(), QColor(220, 230, 237, 156)); });
  scrim.setGeometry(rect());
  scrim.show();
  scrim.raise();
  DashboardDialog dialog(this);
  dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  dialog.setAttribute(Qt::WA_TranslucentBackground);
  dialog.setWindowTitle(tr("WiiTogether Settings"));
  dialog.setMinimumWidth(430);
  dialog.setFont(font());
  dialog.setCursor(m_pointer_enabled ? m_pointer : QCursor(Qt::ArrowCursor));
  dialog.setStyleSheet(QStringLiteral("QDialog { background: transparent; color: #697b86; }"
                                      "QLabel, QCheckBox { color: #697b86; font-size: 16px; "
                                      "background: transparent; border: none; }"
                                      "QCheckBox { padding: 12px 0; }"));
  auto* layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(25, 20, 25, 22);
  auto* heading = new QLabel(tr("WiiTogether Settings"));
  heading->setFont(PixelFont(font(), 25));
  heading->setStyleSheet(QStringLiteral("font-size: 25px;"));
  layout->addWidget(heading);
  auto* animations = new QCheckBox(tr("Menu animations"));
  animations->setChecked(m_motion_enabled);
  auto* pointer = new QCheckBox(tr("Player 1 pointer"));
  pointer->setChecked(m_pointer_enabled);
  layout->addWidget(animations);
  layout->addWidget(pointer);
  auto* dolphin_settings = Pill(&dialog, tr("Dolphin Settings"), 18);
  auto* phones = Pill(&dialog, tr("Connect Phones"), 18);
  auto* library = Pill(&dialog, tr("Game Library"), 18);
  auto* done = Pill(&dialog, tr("Done"), 18, true);
  for (auto* b : {dolphin_settings, phones, library, done})
  {
    b->setMinimumHeight(54);
    layout->addWidget(b);
  }
  connect(animations, &QCheckBox::toggled, this, [this](bool enabled) {
    m_motion_enabled = enabled;
    Settings::GetQSettings().setValue(QStringLiteral("wiitogether/animations"), enabled);
    m_frame_timer.setInterval(enabled ? 33 : 100);
    if (!enabled)
    {
      m_slide.stop();
      m_position = m_selected;
    }
    update();
  });
  connect(pointer, &QCheckBox::toggled, this, [this](bool enabled) {
    m_pointer_enabled = enabled;
    Settings::GetQSettings().setValue(QStringLiteral("wiitogether/pointer"), enabled);
    setCursor(enabled ? m_pointer : QCursor(Qt::ArrowCursor));
  });
  int action = 0;
  connect(dolphin_settings, &QAbstractButton::clicked, &dialog, [&] {
    action = 1;
    dialog.accept();
  });
  connect(phones, &QAbstractButton::clicked, &dialog, [&] {
    action = 2;
    dialog.accept();
  });
  connect(library, &QAbstractButton::clicked, &dialog, [&] {
    action = 3;
    dialog.accept();
  });
  connect(done, &QAbstractButton::clicked, &dialog, &QDialog::accept);
  dialog.exec();
  scrim.hide();
  if (action == 1)
    emit SettingsRequested();
  else if (action == 2)
    emit ConnectPhonesRequested();
  else if (action == 3)
    emit LibraryRequested();
}
