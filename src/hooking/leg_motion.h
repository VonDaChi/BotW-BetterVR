#pragma once

#include <cmath>
#include <array>

namespace LegMotion {

struct Sample {
    float t = 0.0f;
    float hmd_yaw = 0.0f;
    std::array<float, 3> left_pos = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> right_pos = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> left_vel = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> right_vel = {0.0f, 0.0f, 0.0f};
};

struct MotionResult {
    std::array<float, 2> walk_vector = {0.0f, 0.0f};
    bool is_run = false;
    bool is_jump = false;
};

// Reconstruct velocity from position finite differences, skipping frames with
// non-monotonic or excessively large dt. Mirrors tools/leg_motion_lab.py
// reconstruct_velocities. The OpenVR runtime reports zero linear velocity for
// foot poses, so this is the only usable velocity source.
void reconstruct_velocities(Sample* samples, int count);

class LegMotionAnalyzer {
public:
    LegMotionAnalyzer() = default;

    void calibrate(const Sample* samples, int count);
    MotionResult update(const Sample& sample);

    float stand_foot_y() const { return stand_foot_y_; }

private:
        static std::array<float, 2> to_hmd_local_2d(const std::array<float, 3>& velocity, float yaw);

    float walk_speed_threshold_ = 0.12f;
    float run_speed_threshold_ = 0.30f;
    float jump_lift_threshold_ = 0.055f;
    float jump_upward_speed_threshold_ = 0.30f;
    float deadzone_ = 0.08f;
    float smoothing_alpha_ = 0.22f;
    float jump_cooldown_s_ = 0.40f;
    float up_velocity_threshold_ = 0.05f;  // minimum upward velocity to count as a real step, not noise

    float stand_foot_y_ = 0.0f;
    float last_t_ = 0.0f;
    float last_left_y_ = 0.0f;
    float last_right_y_ = 0.0f;
    float last_jump_t_ = 0.0f;
    bool jump_armed_ = false;  // armed only after calibrate() to prevent first-frame false jump
    std::array<float, 2> filtered_ = {0.0f, 0.0f};

    // Phase-based slow-walk detection (P5). Tracks the alternating heel-strike
    // pattern of left/right feet so walking at < 0.1 m/s is still detected.
    float left_phase_ = 0.0f;        // 0..1, advances with each left heel strike
    float right_phase_ = 0.0f;       // 0..1, advances with each right heel strike
    bool left_was_up_ = false;       // left foot was moving up last frame
    bool right_was_up_ = false;      // right foot was moving up last frame
    float step_period_s_ = 0.6f;     // estimated stride period (s)
    float last_left_hs_t_ = 0.0f;    // timestamp of last left heel strike
    float last_right_hs_t_ = 0.0f;   // timestamp of last right heel strike
    int phase_step_count_ = 0;
    bool phase_active_ = false;      // true once alternating steps are seen
    float last_activity_t_ = 0.0f;   // timestamp of last detected activity
    float activity_timeout_s_ = 1.5f; // reset phase detection after this long without activity
};

} // namespace LegMotion
