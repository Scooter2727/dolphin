// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic fixtures: no commercial game data. Run normally for assertions or
// with --preview [opening.bnr] to inspect the production opening widget.
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QPainter>
#include <QTemporaryDir>
#include <QtEndian>
#include <bit>
#include <iostream>
#include <stdexcept>
#include "DolphinQt/WiiBanner.h"
#include "DolphinQt/WiiGameOpening.h"

namespace
{
void Check(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}
void Put(QByteArray& b, int at, quint32 n, int width = 4)
{
  if (b.size() < at + width)
    b.resize(at + width);
  for (int i = 0; i < width; ++i)
    b[at + i] = char(n >> (8 * (width - i - 1)));
}
void Float(QByteArray& b, int at, float f)
{
  Put(b, at, std::bit_cast<quint32>(f));
}
QByteArray Section(const char* tag, int length)
{
  QByteArray b(length, '\0');
  b.replace(qsizetype{0}, 4, tag, 4);
  Put(b, 4, length);
  return b;
}
struct File
{
  QByteArray name, data;
};
QByteArray Archive(const std::vector<File>& files)
{
  // All fixtures below use a known two-level directory tree.
  QByteArray b(32, '\0');
  Put(b, 0, 0x55aa382d);
  Put(b, 4, 32);
  QByteArray strings(1, '\0');
  struct Entry
  {
    int name, offset, size;
    bool dir;
  };
  std::vector<Entry> entries{{0, 0, 0, true}};
  QString current;
  int dir_index = -1;
  for (const auto& file : files)
  {
    const auto split = file.name.lastIndexOf('/');
    const QString dir = QString::fromLatin1(file.name.left(split));
    if (dir != current)
    {
      if (dir_index >= 0)
        entries[dir_index].size = static_cast<int>(entries.size());
      // Paths are supplied as flat subtrees in each separate archive.
      current = dir;
      dir_index = static_cast<int>(entries.size());
      entries.push_back({int(strings.size()), 0, 0, true});
      strings += file.name.left(split);
      strings += '\0';
    }
    entries.push_back({int(strings.size()), 0, int(file.data.size()), false});
    strings += file.name.mid(split + 1);
    strings += '\0';
  }
  if (dir_index >= 0)
    entries[dir_index].size = static_cast<int>(entries.size());
  entries[0].size = static_cast<int>(entries.size());
  int data_at = 32 + int(entries.size()) * 12 + int(strings.size());
  data_at = (data_at + 31) & ~31;
  Put(b, 8, data_at - 32);
  Put(b, 12, data_at);
  b.resize(data_at);
  b.fill('\0', data_at);
  Put(b, 0, 0x55aa382d);
  Put(b, 4, 32);
  Put(b, 8, data_at - 32);
  Put(b, 12, data_at);
  int f = 0;
  for (int i = 0; i < int(entries.size()); ++i)
  {
    auto& e = entries[i];
    if (!e.dir)
    {
      e.offset = b.size();
      b += files[f++].data;
    }
    Put(b, 32 + i * 12, (e.dir ? 0x01000000 : 0) | e.name);
    Put(b, 36 + i * 12, e.offset);
    Put(b, 40 + i * 12, e.size);
  }
  b.replace(32 + int(entries.size()) * 12, strings.size(), strings);
  return b;
}
QByteArray Fixture()
{
  // A red RGB5A3 4x4 TPL texture, an animated centered picture, and silent PCM.
  QByteArray tpl(96, '\0');
  Put(tpl, 0, 0x0020af30);
  Put(tpl, 4, 1);
  Put(tpl, 8, 12);
  Put(tpl, 12, 20);
  Put(tpl, 20, 4, 2);
  Put(tpl, 22, 4, 2);
  Put(tpl, 24, 5);
  Put(tpl, 28, 64);
  for (int i = 64; i < 96; i += 2)
    Put(tpl, i, 0xfc00, 2);
  auto lyt = Section("lyt1", 20);
  Float(lyt, 12, 608);
  Float(lyt, 16, 456);
  auto txl = Section("txl1", 30);
  Put(txl, 8, 1, 2);
  Put(txl, 12, 8);
  txl.replace(20, 8, "tile.tpl", 8);
  auto mat = Section("mat1", 84);
  Put(mat, 8, 1, 2);
  Put(mat, 12, 16);
  Put(mat, 76, 1);
  Put(mat, 80, 0, 2);
  auto pic = Section("pic1", 128);
  pic[8] = 1;
  pic[9] = 4;
  pic[10] = char(-1);
  pic.replace(12, 4, "tile", 4);
  Float(pic, 36, -50);
  Float(pic, 60, 1);
  Float(pic, 64, 1);
  Float(pic, 68, 64);
  Float(pic, 72, 64);
  for (int i = 76; i < 92; ++i)
    pic[i] = char(-1);
  Put(pic, 92, 0, 2);
  pic[94] = 1;
  Float(pic, 104, 1);
  Float(pic, 116, 1);
  Float(pic, 120, 1);
  Float(pic, 124, 1);
  QByteArray layout(16, '\0');
  layout.replace(qsizetype{0}, 4, "RLYT", 4);
  Put(layout, 4, 0xfeff0008);
  Put(layout, 12, 16, 2);
  Put(layout, 14, 4, 2);
  layout += lyt + txl + mat + pic;
  Put(layout, 8, layout.size());
  auto pai = Section("pai1", 112);
  Put(pai, 8, 60, 2);
  Put(pai, 14, 1, 2);
  Put(pai, 16, 20);
  Put(pai, 20, 24);
  pai.replace(24, 4, "tile", 4);
  pai[44] = 1;
  Put(pai, 48, 28);
  pai.replace(52, 4, "RLPA", 4);
  pai[56] = 1;
  Put(pai, 60, 12);
  pai[66] = 2;
  Put(pai, 68, 2, 2);
  Put(pai, 72, 12);
  Float(pai, 76, 0);
  Float(pai, 80, -50);
  Float(pai, 84, 0);
  Float(pai, 88, 60);
  Float(pai, 92, 50);
  Float(pai, 96, 0);
  QByteArray anim(16, '\0');
  anim.replace(qsizetype{0}, 4, "RLAN", 4);
  Put(anim, 4, 0xfeff0008);
  Put(anim, 12, 16, 2);
  Put(anim, 14, 1, 2);
  anim += pai;
  Put(anim, 8, anim.size());
  // Build nested arc directories explicitly because these are also parser tests.
  auto resources = Archive(
      {{"blyt/banner.brlyt", layout}, {"timg/tile.tpl", tpl}, {"anim/banner_start.brlan", anim}});
  // Wrap the flat directory tree with an arc directory by adding one node and
  // adjusting absolute data offsets and string references.
  const int old_count =
      qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(resources.constData() + 40));
  const int old_data =
      qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(resources.constData() + 12));
  QByteArray wrapped = resources;
  wrapped.insert(44, QByteArray(12, '\0'));
  Put(wrapped, 40, old_count + 1);
  // Append arc name to the existing string table before the data region.
  wrapped.insert(old_data + 12, QByteArray("arc\0", 4));
  const int old_string_size = old_data - (32 + old_count * 12);
  Put(wrapped, 44, 0x01000000 | old_string_size);
  Put(wrapped, 48, 0);
  Put(wrapped, 52, old_count + 1);
  for (int i = 2; i <= old_count; ++i)
  {
    const int at = 32 + i * 12;
    const quint32 info =
        qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(wrapped.constData() + at));
    const quint32 off =
        qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(wrapped.constData() + at + 4));
    const quint32 size =
        qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(wrapped.constData() + at + 8));
    if (info >> 24)
    {
      Put(wrapped, at + 4, 1);
      Put(wrapped, at + 8, size + 1);
    }
    else
      Put(wrapped, at + 4, off + 16);
  }
  Put(wrapped, 12, old_data + 16);
  Put(wrapped, 8, old_data + 16 - 32);
  QByteArray opening(0x600, '\0');
  opening.replace(0x40, 4, "IMET", 4);
  const QByteArray wave =
      QByteArray::fromHex("524946462c00000057415645666d74201000000001000100401f0000803e000002001000"
                          "64617461080000000000000000000000");
  opening += Archive({{"meta/banner.bin", wrapped}, {"meta/sound.bin", wave}});
  return opening;
}
}  // namespace

int main(int argc, char** argv)
{
  QApplication app(argc, argv);
  try
  {
    const QByteArray fixture = Fixture();
    const auto scene = WiiBanner::Parse(fixture);
    Check(scene.panes.size() == 1, "Valid fixture did not load a pane");
    Check(scene.textures.size() == 1, "TPL texture not decoded");
    Check(scene.textures[0].pixelColor(0, 0).red() > 240, "RGB5A3 decoding failed");
    Check(scene.intro.tracks.size() == 1, "Animation track not loaded");
    Check(scene.pcm.size() == 8 && scene.sample_rate == 8000 && scene.channels == 1,
          "PCM banner sound did not decode");
    Check(std::abs(scene.intro.tracks[0].Evaluate(30)) < .001, "Hermite interpolation failed");
    QImage image(608, 456, QImage::Format_RGBA8888);
    image.fill(Qt::white);
    {
      QPainter painter(&image);
      scene.Draw(painter, image.rect(), .5);
    }
    Check(image.pixelColor(304, 228).green() < 16,
          "Animated pane did not render at expected position");
    for (int size : {0, 1, 4, 31, 100, int(fixture.size() - 1)})
      Check(WiiBanner::Parse(fixture.left(size)).panes.empty(), "Truncated input was accepted");
    auto corrupt = fixture;
    corrupt[0x600 + 4] = char(-1);
    Check(WiiBanner::Parse(corrupt).panes.empty(), "Invalid archive offset accepted");
    QByteArray compressed = QByteArray::fromHex("4c5a373710");
    for (int i = 0; i < 3; ++i)
      compressed.append(char((fixture.size() >> (i * 8)) & 255));
    for (int i = 0; i < fixture.size(); i += 8)
    {
      compressed.append(char(0));
      compressed.append(fixture.mid(i, 8));
    }
    Check(WiiBanner::Parse(compressed).panes.size() == 1, "LZ77 wrapper failed");
    Check(WiiBanner::Parse(QByteArray::fromHex("4c5a37371010000080f000")).panes.empty(),
          "Invalid LZ77 back-reference accepted");
    auto bad_texture = fixture;
    const int tpl = bad_texture.indexOf(QByteArray::fromHex("0020af30"));
    Check(tpl >= 0, "Missing TPL fixture");
    Put(bad_texture, tpl + 24, 0xffffffff);
    Check(WiiBanner::Parse(bad_texture).panes.empty(), "Invalid texture format accepted");
    Check(WiiBanner::LoadGame(QStringLiteral("missing-game.rvz")).panes.empty(),
          "Missing disc did not fail cleanly");
    std::cout
        << "Banner checks passed: archive, TPL, layout, animation, rendering, invalid input.\n";
    if (app.arguments().contains(QStringLiteral("--inspect")))
    {
      Check(app.arguments().size() >= 3, "Supply a game or opening.bnr path");
      const auto real = WiiBanner::LoadGame(app.arguments()[2]);
      std::cout << "panes=" << real.panes.size() << " textures=" << real.textures.size()
                << " intro=" << real.intro.frames << " loop=" << real.loop.frames
                << " audio=" << real.pcm.size() << " sample_rate=" << real.sample_rate
                << " message=" << real.message.toStdString() << '\n';
      if (app.arguments().size() > 3)
      {
        QImage frame(608, 456, QImage::Format_RGBA8888);
        frame.fill(Qt::white);
        {
          QPainter painter(&frame);
          real.Draw(painter, frame.rect(), 2.0);
        }
        Check(frame.save(app.arguments()[3]), "Could not save diagnostic frame");
      }
      return real.panes.empty() ? 1 : 0;
    }
    if (app.arguments().contains(QStringLiteral("--preview")))
    {
      QTemporaryDir temporary;
      const QString file_path = temporary.filePath(QStringLiteral("opening.bnr"));
      QFile file(file_path);
      Check(file.open(QIODevice::WriteOnly), "Could not save test fixture");
      file.write(fixture);
      file.close();
      const int font_id =
          QFontDatabase::addApplicationFont(QStringLiteral(":/wiitogether/font.ttf"));
      const auto families = QFontDatabase::applicationFontFamilies(font_id);
      WiiGameOpening window;
      if (!families.isEmpty())
        window.setFont(QFont(families.front()));
      window.setWindowTitle(QStringLiteral("WiiTogether Banner Check"));
      window.resize(1000, 720);
      QObject::connect(&window, &WiiGameOpening::BackRequested, &app, &QApplication::quit);
      QObject::connect(&window, &WiiGameOpening::PlayRequested, &app, &QApplication::quit);
      window.show();
      window.Open(app.arguments().size() > 2 ? app.arguments().last() : file_path,
                  QStringLiteral("Synthetic banner test"), {});
      return app.exec();
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
