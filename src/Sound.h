#pragma once

// Short terminal-style beeps, generated in code so there are no sound files.
// If there's no audio device every call is a silent no-op.
enum class Sfx {
  Move,
  Rotate,
  Lock,
  HardDrop,
  Clear,
  Tetris,
  LevelUp,
  GameOver,
  MenuMove,
  MenuSelect,
  Countdown,
  Go,
  Pause,
  Count
};

namespace sound {
void init();
void shutdown();
void play(Sfx effect);
void set_enabled(bool enabled);
bool enabled();
bool available();
}  // namespace sound
