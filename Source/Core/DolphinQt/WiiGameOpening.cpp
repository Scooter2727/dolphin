// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "DolphinQt/WiiGameOpening.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#include <QApplication>
#include <QHideEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QThreadPool>

struct WiiGameOpening::Sound
{
#ifdef _WIN32
  HWAVEOUT device = nullptr;
  std::array<WAVEHDR, 2> headers{};
  ~Sound()
  {
    if (device)
    {
      waveOutReset(device);
      for (auto& header : headers)
        if (header.dwFlags & WHDR_PREPARED)
          waveOutUnprepareHeader(device, &header, sizeof(header));
      waveOutClose(device);
    }
  }
#endif
};

WiiGameOpening::WiiGameOpening(QWidget* parent) : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent);
  setFocusPolicy(Qt::StrongFocus);
  m_background.load(QStringLiteral(":/wiitogether/background.png"));
  m_play = new QPushButton(tr("Play Game"), this);
  m_back = new QPushButton(tr("Back to Menu"), this);
  const QString style = QStringLiteral(
      "QPushButton { color: #69767e; background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
      "stop:0 white,stop:0.55 #f7f9fa,stop:1 #dfe7eb); border:2px solid #b2bfc7;"
      "border-radius:28px; font-size:23px; padding:8px 20px; }"
      "QPushButton:hover, QPushButton:focus { border-color:#32b8e7; }"
      "QPushButton:pressed { background:#d9eef6; }"
      "QPushButton:disabled { color:#a5afb6; }");
  m_play->setStyleSheet(style);
  m_back->setStyleSheet(style);
  m_back->installEventFilter(this);
  m_play->installEventFilter(this);
  connect(m_play, &QPushButton::clicked, this, [this] {
    Stop();
    emit PlayRequested();
  });
  connect(m_back, &QPushButton::clicked, this, [this] {
    Stop();
    emit BackRequested();
  });
  m_timer.setInterval(16);
  connect(&m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
}

WiiGameOpening::~WiiGameOpening()
{
  Stop();
}

void WiiGameOpening::Open(const QString& path, const QString& title, const QPixmap& cover)
{
  Stop();
  m_title = title;
  m_cover = cover;
  m_scene = {};
  m_loading = true;
  m_elapsed.start();
  m_timer.start();
  const int generation = ++m_generation;
  const QPointer<WiiGameOpening> guard(this);
  QThreadPool::globalInstance()->start([guard, path, generation] {
    auto scene = WiiBanner::LoadGame(path);
    QMetaObject::invokeMethod(
        qApp,
        [guard, generation, scene = std::move(scene)]() mutable {
          if (!guard || guard->m_generation != generation)
            return;
          guard->m_scene = std::move(scene);
          guard->m_loading = false;
          guard->m_elapsed.restart();
          guard->StartSound();
          guard->update();
        },
        Qt::QueuedConnection);
  });
  m_play->setFocus();
  update();
}

void WiiGameOpening::Stop()
{
  ++m_generation;  // Ignore background reads completed after Back or another selection.
  m_timer.stop();
  m_sound.reset();  // Unprepare native buffers before replacing the scene's PCM.
}

void WiiGameOpening::SetLaunchEnabled(bool enabled)
{
  m_play->setEnabled(enabled);
}

void WiiGameOpening::StartSound()
{
#ifdef _WIN32
  if (m_scene.pcm.isEmpty())
    return;
  m_sound = std::make_unique<Sound>();
  WAVEFORMATEX format{};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = m_scene.channels;
  format.nSamplesPerSec = m_scene.sample_rate;
  format.wBitsPerSample = 16;
  format.nBlockAlign = format.nChannels * 2;
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
  if (waveOutOpen(&m_sound->device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
  {
    m_sound.reset();
    return;
  }
  const int intro =
      m_scene.loop_sample < 0 ? m_scene.pcm.size() : m_scene.loop_sample * format.nBlockAlign;
  int at = 0;
  if (intro > 0)
  {
    auto& h = m_sound->headers[at++];
    h.lpData = m_scene.pcm.data();
    h.dwBufferLength = intro;
  }
  if (m_scene.loop_sample >= 0)
  {
    auto& h = m_sound->headers[at++];
    h.lpData = m_scene.pcm.data() + intro;
    h.dwBufferLength = m_scene.pcm.size() - intro;
    h.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
    h.dwLoops = 0xffffffff;
  }
  for (int i = 0; i < at; ++i)
  {
    auto& h = m_sound->headers[i];
    if (waveOutPrepareHeader(m_sound->device, &h, sizeof(h)) == MMSYSERR_NOERROR)
      waveOutWrite(m_sound->device, &h, sizeof(h));
  }
#endif
}

void WiiGameOpening::LayoutButtons()
{
  const int button_width = qMin(270, (width() - 100) / 2);
  m_back->setGeometry(width() / 2 - button_width - 18, height() - 100, button_width, 62);
  m_play->setGeometry(width() / 2 + 18, height() - 100, button_width, 62);
}

void WiiGameOpening::paintEvent(QPaintEvent*)
{
  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
  p.fillRect(rect(), QColor(QStringLiteral("#edf0f2")));
  p.drawTiledPixmap(rect(), m_background);
  // Wii channel previews occupy a 4:3 stage above the two bottom controls.
  const double h = qMin(height() - 175.0, (width() - 100.0) * 3 / 4);
  const QRectF stage((width() - h * 4 / 3) / 2, (height() - 140 - h) / 2, h * 4 / 3, h);
  QPainterPath frame;
  frame.addRoundedRect(stage, 16, 16);
  p.setPen(QPen(QColor(QStringLiteral("#b8c9d1")), 2));
  p.setBrush(Qt::white);
  p.drawPath(frame);
  p.save();
  p.setClipPath(frame);
  if (!m_scene.panes.empty())
    m_scene.Draw(p, stage, m_elapsed.elapsed() / 1000.0);
  else
  {
    p.setPen(QColor(QStringLiteral("#697b86")));
    QFont title = font();
    title.setPixelSize(28);
    p.setFont(title);
    p.drawText(stage.adjusted(24, 22, -24, -stage.height() + 70), Qt::AlignCenter, m_title);
    if (!m_cover.isNull())
    {
      const QSize size = m_cover.size().scaled(
          QSize(qRound(stage.width() * .6), qRound(stage.height() * .58)), Qt::KeepAspectRatio);
      p.drawPixmap(QRectF(stage.center() - QPointF(size.width() / 2.0, size.height() / 2.0), size),
                   m_cover, m_cover.rect());
    }
    QFont text = font();
    text.setPixelSize(15);
    p.setFont(text);
    p.drawText(stage.adjusted(24, stage.height() - 75, -24, -15),
               Qt::AlignCenter | Qt::TextWordWrap,
               m_loading ? tr("Opening channel…") : m_scene.message);
  }
  p.restore();
  if (!m_scene.panes.empty() && !m_scene.message.isEmpty())
  {
    p.setPen(QColor(QStringLiteral("#89969f")));
    QFont text = font();
    text.setPixelSize(12);
    p.setFont(text);
    p.drawText(QRect(20, height() - 135, width() - 40, 24), Qt::AlignCenter, m_scene.message);
  }
}

void WiiGameOpening::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  LayoutButtons();
}
void WiiGameOpening::hideEvent(QHideEvent* event)
{
  Stop();
  QWidget::hideEvent(event);
}
void WiiGameOpening::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Backspace)
    m_back->click();
  else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    m_play->click();
  else
    QWidget::keyPressEvent(event);
}

bool WiiGameOpening::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::KeyPress)
  {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Escape || key->key() == Qt::Key_Backspace)
    {
      m_back->click();
      return true;
    }
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
    {
      static_cast<QPushButton*>(object)->click();
      return true;
    }
    if (key->key() == Qt::Key_Left || key->key() == Qt::Key_Right)
    {
      (key->key() == Qt::Key_Left ? m_back : m_play)->setFocus();
      return true;
    }
  }
  return QWidget::eventFilter(object, event);
}
