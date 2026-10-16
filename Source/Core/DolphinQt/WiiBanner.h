// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QRectF>
#include <QString>
#include <array>
#include <vector>

class QPainter;

namespace WiiBanner
{
struct Key
{
  float frame = 0, value = 0, slope = 0;
};
struct Track
{
  QString name;
  QByteArray kind;
  int target = 0;
  bool step = false;
  std::vector<Key> keys;
  float Evaluate(float frame) const;
};
struct Animation
{
  int frames = 0;
  bool loop = false;
  std::vector<Track> tracks;
};
struct Pane
{
  QString name;
  int parent = -1;
  int flags = 1, origin = 0, alpha = 255;
  // Translation xyz, rotation xyz, scale xy, width and height.
  std::array<float, 10> values{0, 0, 0, 0, 0, 0, 1, 1, 0, 0};
  int texture = -1;
  bool picture = false;
  QString text;
  QColor color = Qt::white;
  float font_size = 20;
  QRectF uv{0, 0, 1, 1};
};
struct Scene
{
  QSizeF size{608, 456};
  std::vector<QImage> textures;
  std::vector<Pane> panes;
  Animation intro, loop;
  QByteArray pcm;  // Interleaved little-endian signed 16-bit PCM.
  int sample_rate = 0, channels = 0;
  int loop_sample = -1;
  bool approximate = false;
  QString message;
  void Draw(QPainter& painter, const QRectF& area, double seconds) const;
};

// All offsets, counts, allocations and decompression sizes are bounded. Parsing
// happens off the GUI thread; results own their data and contain no QPixmaps.
Scene Parse(const QByteArray& opening);
Scene LoadGame(const QString& path);
}  // namespace WiiBanner
