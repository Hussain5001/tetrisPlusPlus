#include "Sound.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "raylib.h"

namespace sound {

namespace {
const int kRate = 22050;
bool g_ready = false;
bool g_enabled = true;
::Sound g_sounds[(int)Sfx::Count];

enum class Wavef { Square, Triangle, Noise };

// A tone sliding from f0 to f1 Hz with a quick attack and linear decay
void tone(std::vector<float>& out, float f0, float f1, float seconds,
          Wavef shape, float volume) {
  int n = (int)(seconds * kRate);
  float phase = 0;
  for (int i = 0; i < n; i++) {
    float t = (float)i / n;
    float freq = f0 + (f1 - f0) * t;
    phase += freq / kRate;
    phase -= std::floor(phase);
    float s;
    switch (shape) {
      case Wavef::Square: s = phase < 0.5f ? 1.f : -1.f; break;
      case Wavef::Triangle: s = 4 * std::fabs(phase - 0.5f) - 1; break;
      default: s = (float)std::rand() / RAND_MAX * 2 - 1; break;
    }
    float attack = std::min(1.f, i / (0.004f * kRate));
    out.push_back(s * volume * attack * (1 - t));
  }
}

void silence(std::vector<float>& out, float seconds) {
  out.insert(out.end(), (size_t)(seconds * kRate), 0.f);
}

std::vector<float> make(Sfx effect) {
  std::vector<float> w;
  switch (effect) {
    case Sfx::Move: tone(w, 520, 480, 0.025f, Wavef::Square, 0.10f); break;
    case Sfx::Rotate: tone(w, 700, 900, 0.04f, Wavef::Square, 0.10f); break;
    case Sfx::Lock: tone(w, 180, 90, 0.06f, Wavef::Triangle, 0.35f); break;
    case Sfx::HardDrop:
      tone(w, 0, 0, 0.05f, Wavef::Noise, 0.20f);
      tone(w, 160, 60, 0.09f, Wavef::Triangle, 0.40f);
      break;
    case Sfx::Clear:
      for (float f : {523.f, 659.f, 784.f}) tone(w, f, f, 0.06f, Wavef::Square, 0.14f);
      break;
    case Sfx::Tetris:
      for (float f : {523.f, 659.f, 784.f, 1047.f, 784.f, 1047.f})
        tone(w, f, f, 0.07f, Wavef::Square, 0.15f);
      break;
    case Sfx::LevelUp:
      tone(w, 440, 880, 0.12f, Wavef::Square, 0.12f);
      tone(w, 880, 1320, 0.10f, Wavef::Square, 0.12f);
      break;
    case Sfx::GameOver:
      for (float f : {392.f, 330.f, 262.f, 196.f}) {
        tone(w, f, f * 0.97f, 0.16f, Wavef::Triangle, 0.35f);
        silence(w, 0.03f);
      }
      break;
    case Sfx::MenuMove: tone(w, 880, 880, 0.02f, Wavef::Square, 0.07f); break;
    case Sfx::MenuSelect:
      tone(w, 660, 660, 0.04f, Wavef::Square, 0.10f);
      tone(w, 990, 990, 0.06f, Wavef::Square, 0.10f);
      break;
    case Sfx::Countdown: tone(w, 440, 440, 0.08f, Wavef::Square, 0.10f); break;
    case Sfx::Go: tone(w, 880, 880, 0.16f, Wavef::Square, 0.12f); break;
    case Sfx::Pause: tone(w, 600, 300, 0.10f, Wavef::Triangle, 0.25f); break;
    default: break;
  }
  return w;
}
}  // namespace

void init() {
  InitAudioDevice();
  g_ready = IsAudioDeviceReady();
  if (!g_ready) {
    TraceLog(LOG_WARNING, "No audio device, sounds are off");
    return;
  }
  for (int i = 0; i < (int)Sfx::Count; i++) {
    std::vector<float> samples = make((Sfx)i);
    Wave wave;
    wave.frameCount = (unsigned int)samples.size();
    wave.sampleRate = kRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    short* data = (short*)RL_MALLOC(samples.size() * sizeof(short));
    for (size_t j = 0; j < samples.size(); j++) {
      data[j] = (short)(std::max(-1.f, std::min(1.f, samples[j])) * 32767);
    }
    wave.data = data;
    g_sounds[i] = LoadSoundFromWave(wave);
    UnloadWave(wave);
  }
}

void shutdown() {
  if (!g_ready) return;
  for (::Sound& s : g_sounds) UnloadSound(s);
  CloseAudioDevice();
  g_ready = false;
}

void play(Sfx effect) {
  if (!g_ready || !g_enabled) return;
  PlaySound(g_sounds[(int)effect]);
}

void set_enabled(bool on) { g_enabled = on; }
bool enabled() { return g_enabled; }
bool available() { return g_ready; }

}  // namespace sound
