// Minimal WAV reader/writer: reads PCM16/24/32 and float32, writes float32.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct WavData {
  int sr = 48000;
  int channels = 1;
  std::vector<std::vector<float>> ch;  // ch[c][frame]
  size_t frames() const { return ch.empty() ? 0 : ch[0].size(); }
};

inline bool wav_read(const std::string &path, WavData &w, std::string &err) {
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) { err = "cannot open " + path; return false; }
  auto rd32 = [&](uint32_t &v) { return std::fread(&v, 4, 1, f) == 1; };
  char id[4];
  uint32_t sz;
  if (std::fread(id, 1, 4, f) != 4 || std::memcmp(id, "RIFF", 4) || !rd32(sz) ||
      std::fread(id, 1, 4, f) != 4 || std::memcmp(id, "WAVE", 4)) {
    err = "not a RIFF/WAVE file"; std::fclose(f); return false;
  }
  uint16_t fmt = 0, nch = 0, bits = 0;
  uint32_t sr = 0;
  std::vector<uint8_t> data;
  while (std::fread(id, 1, 4, f) == 4 && rd32(sz)) {
    if (!std::memcmp(id, "fmt ", 4)) {
      std::vector<uint8_t> b(sz);
      if (std::fread(b.data(), 1, sz, f) != sz) break;
      std::memcpy(&fmt, &b[0], 2); std::memcpy(&nch, &b[2], 2);
      std::memcpy(&sr, &b[4], 4); std::memcpy(&bits, &b[14], 2);
      if (fmt == 0xFFFE && sz >= 26) std::memcpy(&fmt, &b[24], 2);  // extensible
    } else if (!std::memcmp(id, "data", 4)) {
      data.resize(sz);
      size_t got = std::fread(data.data(), 1, sz, f);
      data.resize(got);
    } else {
      std::fseek(f, sz + (sz & 1), SEEK_CUR);
    }
  }
  std::fclose(f);
  if (!nch || data.empty()) { err = "missing fmt or data chunk"; return false; }
  int bps = bits / 8;
  size_t n = data.size() / (bps * nch);
  w.sr = (int)sr; w.channels = nch;
  w.ch.assign(nch, std::vector<float>(n));
  for (size_t i = 0; i < n; i++)
    for (int c = 0; c < nch; c++) {
      const uint8_t *p = &data[(i * nch + c) * bps];
      float v = 0;
      if (fmt == 3 && bits == 32) { std::memcpy(&v, p, 4); }
      else if (fmt == 1 && bits == 16) { int16_t s; std::memcpy(&s, p, 2); v = s / 32768.f; }
      else if (fmt == 1 && bits == 24) { int32_t s = (p[0] << 8) | (p[1] << 16) | (p[2] << 24); v = (s >> 8) / 8388608.f; }
      else if (fmt == 1 && bits == 32) { int32_t s; std::memcpy(&s, p, 4); v = s / 2147483648.f; }
      else { err = "unsupported WAV format " + std::to_string(fmt) + "/" + std::to_string(bits); return false; }
      w.ch[c][i] = v;
    }
  return true;
}

inline bool wav_write_f32(const std::string &path, const WavData &w) {
  FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  uint32_t n = (uint32_t)w.frames(), nch = (uint32_t)w.ch.size();
  uint32_t dbytes = n * nch * 4, riff = 4 + 8 + 16 + 8 + dbytes, fmtsz = 16, sr = (uint32_t)w.sr;
  uint16_t fmt = 3, ch16 = (uint16_t)nch, bits = 32, align = (uint16_t)(nch * 4);
  uint32_t brate = sr * align;
  std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVE", 1, 4, f);
  std::fwrite("fmt ", 1, 4, f); std::fwrite(&fmtsz, 4, 1, f); std::fwrite(&fmt, 2, 1, f);
  std::fwrite(&ch16, 2, 1, f); std::fwrite(&sr, 4, 1, f); std::fwrite(&brate, 4, 1, f);
  std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
  std::fwrite("data", 1, 4, f); std::fwrite(&dbytes, 4, 1, f);
  std::vector<float> frame(nch);
  for (uint32_t i = 0; i < n; i++) {
    for (uint32_t c = 0; c < nch; c++) frame[c] = w.ch[c][i];
    std::fwrite(frame.data(), 4, nch, f);
  }
  std::fclose(f);
  return true;
}
