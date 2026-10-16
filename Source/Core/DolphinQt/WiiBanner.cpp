// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "DolphinQt/WiiBanner.h"

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QtEndian>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "Common/Crypto/AES.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "VideoCommon/TextureDecoder.h"

// Format references (independent implementation; no third-party code copied):
// https://wiibrew.org/wiki/Opening.bnr
// https://wiibrew.org/wiki/Wii_Animations
// Wii Banner Player's format declarations as maintained by USB Loader GX:
// https://github.com/wiidev/usbloadergx/tree/enhanced/source/banner
namespace WiiBanner
{
namespace
{
constexpr int LIMIT = 32 * 1024 * 1024;
void Require(bool condition)
{
  if (!condition)
    throw std::runtime_error("Invalid or unsupported Wii banner data");
}
struct Bytes
{
  QByteArray data;
  explicit Bytes(QByteArray bytes) : data(std::move(bytes)) {}
  void Check(qint64 offset, qint64 size) const
  {
    Require(offset >= 0 && size >= 0 && offset <= data.size() && size <= data.size() - offset);
  }
  quint8 U8(qint64 p) const
  {
    Check(p, 1);
    return static_cast<quint8>(data[p]);
  }
  quint16 U16(qint64 p) const
  {
    Check(p, 2);
    return qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(data.constData() + p));
  }
  quint32 U32(qint64 p) const
  {
    Check(p, 4);
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + p));
  }
  float F32(qint64 p) const
  {
    const float f = std::bit_cast<float>(U32(p));
    Require(std::isfinite(f) && std::abs(f) <= 10000000);
    return f;
  }
  QByteArray Slice(qint64 p, qint64 size) const
  {
    Check(p, size);
    return data.mid(p, size);
  }
  QString Name(qint64 p, int max = 256) const
  {
    Check(p, 1);
    const int n = qMin<qint64>(max, data.size() - p);
    QByteArray text = data.mid(p, n);
    const int end = text.indexOf('\0');
    Require(end >= 0);
    return QString::fromLatin1(text.constData(), end);
  }
};

QByteArray Unwrap(QByteArray bytes)
{
  // IMD5 and Nintendo LZ77 wrappers may be nested, but never recurse unboundedly.
  for (int depth = 0; depth < 4; ++depth)
  {
    if (bytes.startsWith("IMD5"))
    {
      Require(bytes.size() >= 32);
      bytes = bytes.mid(32);
      continue;
    }
    if (bytes.startsWith("LZ77"))
      bytes = bytes.mid(4);
    if (bytes.isEmpty() || static_cast<quint8>(bytes[0]) != 0x10)
      return bytes;
    Bytes input(bytes);
    const int length = input.U8(1) | (input.U8(2) << 8) | (input.U8(3) << 16);
    Require(length > 0 && length <= LIMIT);
    QByteArray output;
    output.reserve(length);
    qint64 pos = 4;
    while (output.size() < length)
    {
      const quint8 flags = input.U8(pos++);
      for (int bit = 7; bit >= 0 && output.size() < length; --bit)
      {
        if (!(flags & (1 << bit)))
          output.append(static_cast<char>(input.U8(pos++)));
        else
        {
          const quint8 a = input.U8(pos++), b = input.U8(pos++);
          const int count = (a >> 4) + 3;
          const int distance = ((a & 15) << 8 | b) + 1;
          Require(distance <= output.size() && count <= length - output.size());
          for (int i = 0; i < count; ++i)
            output.append(output[output.size() - distance]);
        }
      }
    }
    bytes = std::move(output);
  }
  Require(false);
  return {};
}

QMap<QString, QByteArray> Archive(QByteArray bytes, bool opening = false)
{
  bytes = Unwrap(std::move(bytes));
  int base = 0;
  if (opening)
  {
    // Disc and NAND IMET headers have different padding. Only inspect aligned
    // candidates in the small metadata header, not arbitrary file contents.
    base = -1;
    for (int p = 0; p <= 0x1000 && p + 4 <= bytes.size(); p += 4)
      if (Bytes(bytes).U32(p) == 0x55aa382d)
      {
        base = p;
        break;
      }
    Require(base >= 0);
    bytes = bytes.mid(base);
  }
  Bytes b(bytes);
  Require(b.U32(0) == 0x55aa382d);
  const qint64 root = b.U32(4);
  b.Check(root, 12);
  const int count = b.U32(root + 8);
  Require(count > 0 && count <= 10000);
  const qint64 strings = root + qint64(count) * 12;
  b.Check(root, qint64(count) * 12);
  QMap<QString, QByteArray> files;
  struct Dir
  {
    int end;
    QString path;
  };
  std::vector<Dir> dirs{{count, QString{}}};
  qint64 total = 0;
  for (int i = 1; i < count; ++i)
  {
    while (!dirs.empty() && i >= dirs.back().end)
      dirs.pop_back();
    Require(!dirs.empty());
    const qint64 node = root + i * 12;
    const quint32 info = b.U32(node);
    const QString name = b.Name(strings + (info & 0xffffff));
    Require(!name.contains(QLatin1Char('/')) && name != QStringLiteral(".."));
    const QString path = dirs.back().path + QLatin1Char('/') + name;
    if (info >> 24)
    {
      const int end = b.U32(node + 8);
      Require(end > i && end <= dirs.back().end && dirs.size() < 64);
      dirs.push_back({end, path});
    }
    else
    {
      const qint64 length = b.U32(node + 8);
      total += length;
      Require(total <= LIMIT);
      files.insert(path, b.Slice(b.U32(node + 4), length));
    }
  }
  return files;
}

QImage Texture(const QByteArray& data)
{
  Bytes b(data);
  Require(b.U32(0) == 0x0020af30 && b.U32(4) >= 1);
  const qint64 table = b.U32(8);
  const qint64 image = b.U32(table), palette = b.U32(table + 4);
  const int height = b.U16(image), width = b.U16(image + 2);
  const quint32 raw_format = b.U32(image + 4);
  Require(raw_format <= 0xe);
  const auto format = static_cast<TextureFormat>(raw_format);
  Require(width > 0 && height > 0 && width <= 2048 && height <= 2048 &&
          IsValidTextureFormat(format));
  const int bw = TexDecoder_GetBlockWidthInTexels(format);
  const int bh = TexDecoder_GetBlockHeightInTexels(format);
  const int padded_w = (width + bw - 1) / bw * bw;
  const int padded_h = (height + bh - 1) / bh * bh;
  const int length = TexDecoder_GetTextureSizeInBytes(padded_w, padded_h, format);
  const QByteArray pixels = b.Slice(b.U32(image + 8), length);
  QByteArray colors;
  TLUTFormat palette_format = TLUTFormat::IA8;
  if (IsColorIndexed(format))
  {
    Require(palette != 0);
    Require(b.U32(palette + 4) <= 2);
    palette_format = static_cast<TLUTFormat>(b.U32(palette + 4));
    const int expected = TexDecoder_GetPaletteSize(format);
    Require(b.U16(palette) * 2 >= expected);
    colors = b.Slice(b.U32(palette + 8), expected);
  }
  QImage result(padded_w, padded_h, QImage::Format_RGBA8888);
  Require(!result.isNull());
  TexDecoder_Decode(result.bits(), reinterpret_cast<const u8*>(pixels.constData()), padded_w,
                    padded_h, format, reinterpret_cast<const u8*>(colors.constData()),
                    palette_format);
  return result.copy(0, 0, width, height);
}

Animation ReadAnimation(const QByteArray& data, bool& approximate)
{
  Bytes b(Unwrap(data));
  Require(b.Slice(0, 4) == "RLAN" && b.U16(4) == 0xfeff);
  qint64 pos = b.U16(12);
  const int sections = b.U16(14);
  Require(sections <= 1024);
  Animation anim;
  int total_tracks = 0;
  int total_keys = 0;
  for (int s = 0; s < sections; ++s)
  {
    const quint32 size = b.U32(pos + 4);
    Require(size >= 8);
    b.Check(pos, size);
    if (b.Slice(pos, 4) == "pai1")
    {
      anim.frames = b.U16(pos + 8);
      anim.loop = b.U8(pos + 10) != 0;
      const int count = b.U16(pos + 14);
      const qint64 table = pos + b.U32(pos + 16);
      Require(count <= 2048);
      for (int i = 0; i < count; ++i)
      {
        const qint64 entry = pos + b.U32(table + i * 4);
        const QString name = b.Name(entry, 20);
        const int tag_count = b.U8(entry + 20);
        const bool material = b.U8(entry + 21) != 0;
        for (int t = 0; t < tag_count; ++t)
        {
          const qint64 tag = entry + b.U32(entry + 24 + t * 4);
          const QByteArray kind = b.Slice(tag, 4);
          const int channels = b.U8(tag + 4);
          for (int c = 0; c < channels; ++c)
          {
            Require(++total_tracks <= 10000);
            const qint64 track_pos = tag + b.U32(tag + 8 + c * 4);
            Track track;
            track.name = name;
            track.kind = kind;
            track.target = b.U8(track_pos + 1);
            track.step = b.U8(track_pos + 2) == 1;
            const int keys = b.U16(track_pos + 4);
            Require(keys > 0 && keys <= 20000);
            total_keys += keys;
            Require(total_keys <= 262144);
            if (material || (kind != "RLPA" && kind != "RLVI" && kind != "RLVC"))
            {
              approximate = true;
              continue;
            }
            if (kind == "RLVC" && track.target != 16)
              approximate = true;
            Require(b.U8(track_pos + 2) == 1 || b.U8(track_pos + 2) == 2);
            const qint64 key_base = track_pos + b.U32(track_pos + 8);
            for (int k = 0; k < keys; ++k)
            {
              const qint64 key = key_base + k * (track.step ? 8 : 12);
              track.keys.push_back({b.F32(key), track.step ? float(b.U8(key + 5)) : b.F32(key + 4),
                                    track.step ? 0 : b.F32(key + 8)});
            }
            std::stable_sort(track.keys.begin(), track.keys.end(),
                             [](const Key& a, const Key& z) { return a.frame < z.frame; });
            anim.tracks.push_back(std::move(track));
            Require(anim.tracks.size() <= 10000);
          }
        }
      }
    }
    pos += size;
  }
  return anim;
}

void Layout(Scene& scene, const QMap<QString, QByteArray>& archive)
{
  Require(archive.contains(QStringLiteral("/arc/blyt/banner.brlyt")));
  Bytes b(archive.value(QStringLiteral("/arc/blyt/banner.brlyt")));
  Require(b.Slice(0, 4) == "RLYT" && b.U16(4) == 0xfeff);
  const int sections = b.U16(14);
  Require(sections <= 8192);
  qint64 pos = b.U16(12);
  QStringList texture_names;
  std::vector<int> materials;
  std::vector<int> parents;
  int last = -1;
  for (int s = 0; s < sections; ++s)
  {
    const qint64 length = b.U32(pos + 4);
    Require(length >= 8);
    Bytes section(b.Slice(pos, length));
    const QByteArray kind = section.Slice(0, 4);
    if (kind == "lyt1")
    {
      scene.size = QSizeF(section.F32(12), section.F32(16));
      Require(scene.size.width() > 0 && scene.size.height() > 0 && scene.size.width() <= 4096 &&
              scene.size.height() <= 4096);
    }
    else if (kind == "txl1")
    {
      const int count = section.U16(8);
      Require(count <= 256);
      qint64 texture_bytes = 0;
      for (int i = 0; i < count; ++i)
      {
        const QString name = section.Name(12 + section.U32(12 + i * 8));
        texture_names.append(name);
        QImage image = Texture(archive.value(QStringLiteral("/arc/timg/") + name));
        texture_bytes += image.sizeInBytes();
        Require(texture_bytes <= 128 * 1024 * 1024);
        scene.textures.push_back(std::move(image));
      }
    }
    else if (kind == "mat1")
    {
      const int count = section.U16(8);
      Require(count <= 1024);
      for (int i = 0; i < count; ++i)
      {
        const qint64 mat = section.U32(12 + i * 4);
        const quint32 flags = section.U32(mat + 60);
        const int textures = flags & 15;
        materials.push_back(textures ? section.U16(mat + 64) : -1);
        if (textures > 1 || ((flags >> 17) & 7) != 0)
          scene.approximate = true;
      }
    }
    else if (kind == "pas1")
    {
      Require(last >= 0 && parents.size() < 64);
      parents.push_back(last);
    }
    else if (kind == "pae1")
    {
      Require(!parents.empty());
      parents.pop_back();
    }
    else if (kind == "pan1" || kind == "pic1" || kind == "txt1" || kind == "bnd1" || kind == "wnd1")
    {
      Pane pane;
      pane.name = section.Name(12, 16);
      pane.parent = parents.empty() ? -1 : parents.back();
      pane.flags = section.U8(8);
      pane.origin = section.U8(9);
      Require(pane.origin < 9);
      pane.alpha = section.U8(10);
      for (int i = 0; i < 10; ++i)
        pane.values[i] = section.F32(36 + i * 4);
      if (kind == "pic1")
      {
        pane.picture = true;
        const int material = section.U16(92);
        Require(material < static_cast<int>(materials.size()));
        pane.texture = materials[material];
        pane.color = QColor(section.U8(76), section.U8(77), section.U8(78), section.U8(79));
        if (section.U8(94) > 0)
        {
          const float left = section.F32(96), top = section.F32(100);
          const float right = section.F32(104), bottom = section.F32(116);
          pane.uv = QRectF(left, top, right - left, bottom - top);
          if (left < 0 || top < 0 || right > 1 || bottom > 1)
            scene.approximate = true;
        }
      }
      else if (kind == "txt1")
      {
        const int count = section.U16(78) / 2;
        const qint64 text = section.U32(88);
        Require(count <= 4096);
        for (int i = 0; i < count; ++i)
        {
          const auto ch = section.U16(text + i * 2);
          if (!ch)
            break;
          pane.text.append(QChar(ch));
        }
        pane.color = QColor(section.U8(92), section.U8(93), section.U8(94), section.U8(95));
        pane.font_size = qBound(1.0f, section.F32(104), 256.0f);
        scene.approximate = true;  // Desktop font substituted for Wii bitmap fonts.
      }
      else if (kind == "wnd1")
        scene.approximate = true;
      last = static_cast<int>(scene.panes.size());
      scene.panes.push_back(std::move(pane));
      Require(scene.panes.size() <= 2048);
    }
    pos += length;
  }
  Require(!scene.panes.empty());
}

void Audio(Scene& scene, const QByteArray& bytes)
{
  Bytes b(Unwrap(bytes));
  if (b.Slice(0, 4) == "RIFF")
  {
    Require(b.Slice(8, 4) == "WAVE");
    qint64 p = 12;
    bool format = false;
    while (p + 8 <= b.data.size())
    {
      const auto id = b.Slice(p, 4);
      const quint32 size =
          qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(b.data.constData() + p + 4));
      b.Check(p + 8, size);
      if (id == "fmt ")
      {
        const QByteArray fmt = b.Slice(p + 8, size);
        Require(fmt.size() >= 16);
        const auto u16 = [&](int at) {
          return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(fmt.constData() + at));
        };
        Require(u16(0) == 1 && u16(14) == 16);
        scene.channels = u16(2);
        scene.sample_rate =
            qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(fmt.constData() + 4));
        Require(scene.channels >= 1 && scene.channels <= 2 && scene.sample_rate >= 8000 &&
                scene.sample_rate <= 96000);
        format = true;
      }
      else if (id == "data")
        scene.pcm = b.Slice(p + 8, size);
      p += 8 + size + (size & 1);
    }
    Require(format && !scene.pcm.isEmpty() && scene.pcm.size() % (scene.channels * 2) == 0);
    return;
  }
  Require(b.Slice(0, 4) == "BNS " && b.U32(4) == 0xfeff0100);
  qint64 info = 0, data = 0;
  const int chunks = b.U16(14);
  Require(chunks <= 16);
  for (int i = 0; i < chunks; ++i)
  {
    const qint64 offset = b.U32(16 + i * 8);
    b.Check(offset, b.U32(20 + i * 8));
    if (b.Slice(offset, 4) == "INFO")
      info = offset + 8;
    if (b.Slice(offset, 4) == "DATA")
      data = offset + 8;
  }
  Require(info != 0 && data != 0 && b.U8(info) == 0);
  scene.channels = b.U8(info + 2);
  scene.sample_rate = b.U16(info + 4);
  const int samples = b.U32(info + 12);
  Require(scene.channels >= 1 && scene.channels <= 2 && scene.sample_rate >= 8000 &&
          scene.sample_rate <= 96000 && samples > 0 && samples <= scene.sample_rate * 120);
  Require(qint64(samples) * scene.channels * 2 <= LIMIT);
  if (b.U8(info + 1))
  {
    scene.loop_sample = b.U32(info + 8);
    Require(scene.loop_sample >= 0 && scene.loop_sample < samples);
  }
  scene.pcm.resize(samples * scene.channels * 2);
  const qint64 table = info + b.U32(info + 16);
  for (int channel = 0; channel < scene.channels; ++channel)
  {
    const qint64 channel_info = info + b.U32(table + channel * 4);
    const qint64 audio = data + b.U32(channel_info);
    const qint64 dsp = info + b.U32(channel_info + 4);
    std::array<int, 16> coefs;
    for (int i = 0; i < 16; ++i)
      coefs[i] = static_cast<qint16>(b.U16(dsp + i * 2));
    int h1 = static_cast<qint16>(b.U16(dsp + 36));
    int h2 = static_cast<qint16>(b.U16(dsp + 38));
    for (int sample = 0; sample < samples; ++sample)
    {
      const qint64 block = audio + (sample / 14) * 8;
      const quint8 control = b.U8(block);
      const int predictor = control >> 4;
      Require(predictor < 8);
      const int nibble_pos = sample % 14;
      const quint8 byte = b.U8(block + 1 + nibble_pos / 2);
      int nibble = nibble_pos % 2 ? byte & 15 : byte >> 4;
      if (nibble >= 8)
        nibble -= 16;
      const qint64 decoded =
          (qint64(nibble) * (qint64{1} << (control & 15)) * 2048 +
           qint64(coefs[predictor * 2]) * h1 + qint64(coefs[predictor * 2 + 1]) * h2 + 1024) >>
          11;
      const qint16 value = static_cast<qint16>(qBound(qint64{-32768}, decoded, qint64{32767}));
      qToLittleEndian<quint16>(
          static_cast<quint16>(value),
          reinterpret_cast<uchar*>(scene.pcm.data() + (sample * scene.channels + channel) * 2));
      h2 = h1;
      h1 = value;
    }
  }
}
}  // namespace

float Track::Evaluate(float frame) const
{
  if (keys.empty())
    return 0;
  const auto next = std::upper_bound(keys.begin(), keys.end(), frame,
                                     [](float f, const Key& k) { return f < k.frame; });
  if (next == keys.begin())
    return next->value;
  const auto& previous = *(next - 1);
  if (step || next == keys.end())
    return previous.value;
  const float span = next->frame - previous.frame;
  if (span <= 0)
    return previous.value;
  const float t = qBound(0.0f, (frame - previous.frame) / span, 1.0f);
  const float t2 = t * t, t3 = t2 * t;
  return (2 * t3 - 3 * t2 + 1) * previous.value + (t3 - 2 * t2 + t) * span * previous.slope +
         (-2 * t3 + 3 * t2) * next->value + (t3 - t2) * span * next->slope;
}

Scene Parse(const QByteArray& opening)
{
  Scene scene;
  try
  {
    Require(!opening.isEmpty() && opening.size() <= LIMIT);
    const auto root = Archive(opening, true);
    const auto sound = root.value(QStringLiteral("/meta/sound.bin"));
    if (!sound.isEmpty())
    {
      try
      {
        Audio(scene, sound);
      }
      catch (const std::exception&)
      {
        scene.pcm.clear();
        scene.approximate = true;
      }
    }
    const auto resources = Archive(root.value(QStringLiteral("/meta/banner.bin")));
    Layout(scene, resources);
    const auto start = resources.value(QStringLiteral("/arc/anim/banner_start.brlan"));
    const auto loop = resources.value(QStringLiteral("/arc/anim/banner_loop.brlan"));
    const auto single = resources.value(QStringLiteral("/arc/anim/banner.brlan"));
    if (!start.isEmpty())
      scene.intro = ReadAnimation(start, scene.approximate);
    if (!loop.isEmpty())
      scene.loop = ReadAnimation(loop, scene.approximate);
    if (start.isEmpty() && loop.isEmpty() && !single.isEmpty())
    {
      Animation anim = ReadAnimation(single, scene.approximate);
      if (anim.loop)
        scene.loop = std::move(anim);
      else
        scene.intro = std::move(anim);
    }
    scene.message =
        scene.approximate ?
            QStringLiteral("Original banner preview — some effects are not supported yet") :
            QString{};
  }
  catch (const std::exception&)
  {
    scene.panes.clear();
    scene.message =
        QStringLiteral("This banner is not supported yet. You can still play the game.");
  }
  return scene;
}

Scene LoadGame(const QString& path)
{
  Scene scene;
  try
  {
    // Also accept an extracted opening.bnr for local compatibility testing.
    if (QFileInfo(path).suffix().compare(QStringLiteral("bnr"), Qt::CaseInsensitive) == 0)
    {
      QFile file(path);
      Require(file.open(QIODevice::ReadOnly) && file.size() <= LIMIT);
      return Parse(file.readAll());
    }
    const auto volume = DiscIO::CreateVolume(path.toStdString());
    Require(volume != nullptr);
    QByteArray opening;
    const auto partition = volume->GetGamePartition();
    if (const auto* fs = volume->GetFileSystem(partition))
    {
      const auto file = fs->FindFileInfo("opening.bnr");
      if (file && !file->IsDirectory())
      {
        Require(file->GetSize() <= LIMIT);
        opening.resize(file->GetSize());
        Require(DiscIO::ReadFile(*volume, partition, file.get(),
                                 reinterpret_cast<u8*>(opening.data()),
                                 opening.size()) == static_cast<u64>(opening.size()));
      }
    }
    else if (volume->GetVolumeType() == DiscIO::Platform::WiiWAD)
    {
      IOS::ES::Content content;
      Require(volume->GetTMD(partition).GetContent(0, &content) && content.size <= LIMIT - 64);
      const auto bytes = volume->GetContent(0);
      Require(!bytes.empty() && bytes.size() <= LIMIT && (bytes.size() % 16) == 0);
      const auto key = volume->GetTicket(partition).GetTitleKey();
      const auto aes = Common::AES::CreateContextDecrypt(key.data());
      opening.resize(bytes.size());
      Require(aes &&
              aes->CryptIvZero(bytes.data(), reinterpret_cast<u8*>(opening.data()), bytes.size()));
      opening.truncate(content.size);
    }
    if (opening.isEmpty())
    {
      scene.message = QStringLiteral("No Wii channel intro is available for this game.");
      return scene;
    }
    return Parse(opening);
  }
  catch (const std::exception&)
  {
    scene.message = QStringLiteral("The game's channel intro could not be read.");
    return scene;
  }
}

void Scene::Draw(QPainter& painter, const QRectF& area, double seconds) const
{
  if (panes.empty())
    return;
  auto frame_panes = panes;
  const double frame = seconds * 60;
  const auto apply = [&](const Animation& anim, float time) {
    for (const auto& track : anim.tracks)
    {
      const auto it = std::find_if(frame_panes.begin(), frame_panes.end(),
                                   [&](const Pane& p) { return p.name == track.name; });
      if (it == frame_panes.end())
        continue;
      const float value = track.Evaluate(time);
      if (track.kind == "RLPA" && track.target < 10)
        it->values[track.target] = value;
      else if (track.kind == "RLVI")
        it->flags = (it->flags & ~1) | (value != 0 ? 1 : 0);
      else if (track.kind == "RLVC" && track.target == 16)
        it->alpha = qBound(0, qRound(value), 255);
    }
  };
  apply(intro, qMin<double>(frame, intro.frames));
  if (frame >= intro.frames && loop.frames > 0)
    apply(loop, loop.loop ? std::fmod(frame - intro.frames, loop.frames) :
                            qMin<double>(frame - intro.frames, loop.frames));
  painter.save();
  painter.setClipRect(area);
  const double scale = qMin(area.width() / size.width(), area.height() / size.height());
  painter.translate(area.center());
  painter.scale(scale, scale);
  std::vector<QTransform> transforms;
  std::vector<double> alpha;
  std::vector<bool> visible;
  for (const auto& pane : frame_panes)
  {
    QTransform transform;
    double opacity = pane.alpha / 255.0;
    bool shown = (pane.flags & 1) != 0;
    if (pane.parent >= 0)
    {
      transform = transforms[pane.parent];
      opacity *= alpha[pane.parent];
      shown = shown && visible[pane.parent];
    }
    const auto& v = pane.values;
    transform.translate(v[0], -v[1]);
    transform.rotate(-v[5]);
    transform.scale(v[6] * std::cos(v[4] * 3.141592653589793 / 180),
                    v[7] * std::cos(v[3] * 3.141592653589793 / 180));
    transforms.push_back(transform);
    alpha.push_back(opacity);
    visible.push_back(shown);
    if (!shown || opacity <= 0)
      continue;
    painter.save();
    painter.setTransform(transform, true);
    painter.setOpacity(opacity * pane.color.alphaF());
    const QRectF rect(-v[8] * (pane.origin % 3) / 2, -v[9] * (pane.origin / 3) / 2, v[8], v[9]);
    if (pane.picture)
    {
      if (pane.texture >= 0 && pane.texture < static_cast<int>(textures.size()))
      {
        const auto& image = textures[pane.texture];
        const QRectF source(pane.uv.x() * image.width(), pane.uv.y() * image.height(),
                            pane.uv.width() * image.width(), pane.uv.height() * image.height());
        painter.drawImage(rect, image, source);
      }
      else
        painter.fillRect(rect, pane.color);
    }
    else if (!pane.text.isEmpty())
    {
      painter.setPen(pane.color);
      QFont font = painter.font();
      font.setPixelSize(qRound(pane.font_size));
      painter.setFont(font);
      painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, pane.text);
    }
    painter.restore();
  }
  painter.restore();
}
}  // namespace WiiBanner
