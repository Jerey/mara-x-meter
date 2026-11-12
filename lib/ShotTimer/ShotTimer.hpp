#pragma once
#include <stdint.h>

/**
 * @brief Shot timer driven by the pump activity.
 *
 * The reed sensor on the vibration pump toggles while the pump runs, so "pump active" is a stream of short pulses
 * rather than a steady level. Every observed pulse is reported via noteActivity() (from a polled pin, an interrupt
 * or the pump field of newer Mara X firmware) and update() is called regularly. The shot starts with the first pulse
 * and ends once no pulse has been seen for stopThresholdMs. The recorded duration runs from the first to the last
 * pulse, so the stop threshold does not inflate it.
 */
class ShotTimer {
 public:
  /**
   * @param stopThresholdMs Silence on the pump signal after which the shot counts as finished.
   * @param minShotMs Shots shorter than this are not kept as "last shot". The Mara X occasionally runs the pump by
   *                  itself for a moment to refresh the heat exchanger water, which would otherwise show up as a two
   *                  second shot.
   */
  ShotTimer(uint32_t stopThresholdMs, uint32_t minShotMs);

  /**
   * @brief Reports that the pump was seen active at the given time stamp.
   *
   * The time stamp may be slightly newer than the nowMs of the following update(), as an interrupt can fire
   * between taking the time and polling.
   */
  void noteActivity(uint32_t atMs);

  /// @brief Advances the state machine. Call at least every few hundred milliseconds.
  void update(uint32_t nowMs);

  bool running() const { return running_; }

  /// @brief Milliseconds since the shot started while running, otherwise the duration of the last shot.
  uint32_t elapsedMs(uint32_t nowMs) const;

  /// @brief Time stamp of the start of the running or last shot.
  uint32_t startedAtMs() const { return startedAt_; }

  /// @brief Duration of the most recent pump run, including runs too short to count as a shot.
  uint32_t lastRunMs() const { return lastRunMs_; }

  bool hasLastShot() const { return hasLastShot_; }
  uint32_t lastShotMs() const { return lastShotMs_; }
  uint32_t lastShotStartedAtMs() const { return lastShotStartedAt_; }

  /// @brief True once after the pump started. Reading clears the flag.
  bool takeStartedEvent();
  /// @brief True once after the pump stopped, regardless of how long it ran. Reading clears the flag.
  bool takeStoppedEvent();
  /// @brief True once after a pump run long enough to count as a shot ended. Reading clears the flag.
  bool takeShotFinishedEvent();

 private:
  const uint32_t stopThresholdMs_;
  const uint32_t minShotMs_;
  bool running_ = false;
  uint32_t startedAt_ = 0;
  uint32_t lastActivityAt_ = 0;
  uint32_t lastRunMs_ = 0;
  bool hasLastShot_ = false;
  uint32_t lastShotMs_ = 0;
  uint32_t lastShotStartedAt_ = 0;
  bool startedEvent_ = false;
  bool stoppedEvent_ = false;
  bool shotFinishedEvent_ = false;
};
