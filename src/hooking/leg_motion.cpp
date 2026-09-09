#include "leg_motion.h"

#include <algorithm>

namespace LegMotion {

// Reconstruct velocity from position finite differences, skipping frames with
// non-monotonic or excessively large dt. Mirrors tools/leg_motion_lab.py
// reconstruct_velocities.
void reconstruct_velocities(Sample* samples, int count) {
    if (count < 2) return;
    for (int i = 1; i < count; ++i) {
        const float dt = samples[i].t - samples[i - 1].t;
        if (dt <= 0.0f || dt > 0.25f) continue;
        for (int j = 0; j < 3; ++j) {
            samples[i].left_vel[j] = (samples[i].left_pos[j] - samples[i - 1].left_pos[j]) / dt;
            samples[i].right_vel[j] = (samples[i].right_pos[j] - samples[i - 1].right_pos[j]) / dt;
        }
    }
}

float horizontal_speed(const std::array<float, 3>& velocity) {
    return std::sqrt(velocity[0] * velocity[0] + velocity[2] * velocity[2]);
}

float yaw_delta(float a, float b) {
    const float two_pi = 2.0f * 3.14159265358979323846f;
    const float pi = 3.14159265358979323846f;
    return std::fmod(b - a + pi, two_pi) - pi;
}

void LegMotionAnalyzer::calibrate(const Sample* samples, int count) {
    if (count <= 0) return;
    float sum = 0.0f;
    for (int i = 0; i < count; ++i) {
        sum += (samples[i].left_pos[1] + samples[i].right_pos[1]) * 0.5f;
    }
    stand_foot_y_ = sum / count;
    jump_armed_ = true;  // arm jump detection only after calibration baseline is established
}

std::array<float, 2> LegMotionAnalyzer::to_hmd_local_2d(const std::array<float, 3>& velocity, float yaw) {
    // Room-space -> HMD-local: velocity is (x, y, z) with Y-up.
    // right = (cos, 0,  sin), forward = (sin, 0, -cos) in room space,
    // so right_comp = vx*cos + vz*sin, forward_comp = vx*sin - vz*cos.
    // Result: {right, forward}, where forward is the HMD's facing direction.
    const float cos_y = std::cos(yaw);
    const float sin_y = std::sin(yaw);
    return {
        velocity[0] * cos_y + velocity[2] * sin_y,
        velocity[0] * sin_y - velocity[2] * cos_y,
    };
}

MotionResult LegMotionAnalyzer::update(const Sample& sample) {
    const auto left_local = to_hmd_local_2d(sample.left_vel, sample.hmd_yaw);
    const auto right_local = to_hmd_local_2d(sample.right_vel, sample.hmd_yaw);
    const float left_speed = std::sqrt(left_local[0] * left_local[0] + left_local[1] * left_local[1]);
    const float right_speed = std::sqrt(right_local[0] * right_local[0] + right_local[1] * right_local[1]);

    // During an in-place step one foot moves forward while the other pushes back.
    // Select the stronger foot and only treat its backward impulse as locomotion.
    const std::array<float, 2>* active = left_speed >= right_speed ? &left_local : &right_local;
    const float active_speed = std::max(left_speed, right_speed);
    const float backward = std::max(0.0f, -(*active)[1]);
    std::array<float, 2> target = {0.0f, 0.0f};
    if (active_speed >= walk_speed_threshold_) {
        target = {(*active)[0], backward};
        const float magnitude = std::sqrt(target[0] * target[0] + target[1] * target[1]);
        if (magnitude > 0.0f) {
            const float scale = std::min(1.0f, magnitude / run_speed_threshold_);
            target[0] *= scale;
            target[1] *= scale;
        }
        if (std::sqrt(target[0] * target[0] + target[1] * target[1]) < deadzone_) {
            target = {0.0f, 0.0f};
        }
    }

    const float a = smoothing_alpha_;
    filtered_[0] += a * (target[0] - filtered_[0]);
    filtered_[1] += a * (target[1] - filtered_[1]);

    const float run_metric = std::max(left_speed, right_speed);
    const bool is_run = run_metric >= run_speed_threshold_;

    // Both feet must rise together: take the LOWER foot's lift so stepping on
    // one foot (walk/run) can never satisfy the gate. Hysteresis re-arms only
    // after both feet have clearly returned to the floor (landed).
    const float dt = (last_t_ <= 0.0f) ? 0.0f : std::max(0.0f, sample.t - last_t_);
    const float left_up = (dt <= 0.0f) ? 0.0f : (sample.left_pos[1] - last_left_y_) / dt;
    const float right_up = (dt <= 0.0f) ? 0.0f : (sample.right_pos[1] - last_right_y_) / dt;
    const float lift_both = std::min(sample.left_pos[1], sample.right_pos[1]) - stand_foot_y_;
    if (lift_both < jump_lift_threshold_ * 0.5f) {
        jump_armed_ = true;
    }
    const bool jump_candidate = (
        lift_both >= jump_lift_threshold_
        && std::min(left_up, right_up) >= jump_upward_speed_threshold_
    );
    const bool is_jump = (
        jump_candidate
        && jump_armed_
        && sample.t - last_jump_t_ >= jump_cooldown_s_
    );
    if (is_jump) {
        last_jump_t_ = sample.t;
        jump_armed_ = false;
    }

    last_t_ = sample.t;
    last_left_y_ = sample.left_pos[1];
    last_right_y_ = sample.right_pos[1];

    // --- Phase-based slow-walk detection (P5) ---
    // When the walk speed is below walk_speed_threshold_ the velocity-based
    // detector falls silent. But even a slow walk has an alternating heel-strike
    // pattern: one foot lifts while the other plants. Track the up/down phase of
    // each foot and use the alternation to synthesise a slow-walk signal.
    // A minimum upward velocity (up_velocity_threshold_) gates "moving up" to
    // reject foot-Y noise during standing (std ~0.0002 m, which otherwise
    // produces 150+ false heel strikes in 30s).
    const bool left_up_now = (left_up > up_velocity_threshold_);
    const bool right_up_now = (right_up > up_velocity_threshold_);
    const float kPhaseStep = 0.02f; // phase increment per frame while foot is up

    if (left_up_now && !left_was_up_) {
        left_phase_ = 0.0f;
    }
    if (right_up_now && !right_was_up_) {
        right_phase_ = 0.0f;
    }
    if (left_up_now) left_phase_ = std::min(1.0f, left_phase_ + kPhaseStep);
    if (right_up_now) right_phase_ = std::min(1.0f, right_phase_ + kPhaseStep);

    // A heel strike is detected when a foot that was moving up suddenly stops
    // rising (or drops). This is the phase zero-crossing.
    const bool left_heel_strike = left_was_up_ && !left_up_now;
    const bool right_heel_strike = right_was_up_ && !right_up_now;
    const bool has_activity = (left_heel_strike || right_heel_strike);

    // Reset the phase detector if no activity is detected for a while
    // (e.g., after upper-body waving moves the trackers). This prevents
    // phase_active_ from persisting after the user stops walking.
    if (has_activity) {
        last_activity_t_ = sample.t;
    } else if (phase_active_ && (sample.t - last_activity_t_) > activity_timeout_s_) {
        phase_active_ = false;
        phase_step_count_ = 0;
        last_activity_t_ = sample.t;
    }

    if (left_heel_strike) {
        phase_step_count_++;
        if (phase_step_count_ >= 2 && !phase_active_) {
            phase_active_ = true;
        }
    }
    if (right_heel_strike) {
        phase_step_count_++;
        if (phase_step_count_ >= 2 && !phase_active_) {
            phase_active_ = true;
        }
    }

    // Estimate stride period from the time between consecutive heel strikes
    // of the SAME foot (left_heel_strike -> next left_heel_strike is one full
    // gait cycle). This replaces the old dt*2.0 approach which used inter-frame
    // time and produced ~0.05s instead of the real ~0.6-1.0s step period.
    if (left_heel_strike) {
        if (last_left_hs_t_ > 0.0f) {
            step_period_s_ = std::max(0.3f, std::min(1.2f, sample.t - last_left_hs_t_));
        }
        last_left_hs_t_ = sample.t;
    }
    if (right_heel_strike) {
        if (last_right_hs_t_ > 0.0f) {
            step_period_s_ = std::max(0.3f, std::min(1.2f, sample.t - last_right_hs_t_));
        }
        last_right_hs_t_ = sample.t;
    }

    // Synthesise a slow-walk vector when the phase detector is active but the
    // velocity detector is silent. For in-place stepping there is no backward
    // impulse (feet stay roughly under the body), so we synthesise forward
    // motion purely from the step rhythm + HMD heading. This is the P5 fix
    // for true in-place walking.
    if (phase_active_ && active_speed < walk_speed_threshold_ && !is_run && !is_jump) {
        // Cadence score: 1.0 when steps are at the natural cadence (~1 Hz),
        // tapering at very fast or very slow rates. Reuse step_period_s_
        // when we have a measurement, otherwise assume default cadence.
        const float assumed_period = (step_period_s_ > 0.0f) ? step_period_s_ : 0.8f;
        const float cadence_score = std::min(1.0f, assumed_period / 0.6f); // 0.6s => 1.0, 1.2s => 0.5
        const float slow_walk = cadence_score * 0.35f; // slow walk ≈ 0.35 of full walk

        // Forward direction: use the active foot's lateral component as a
        // very weak strafe cue, but the dominant forward sign comes from
        // step alternation itself — when phase_active_ fires we assume the
        // user is walking forward. A sideways intent would be reflected in
        // the foot's lateral velocity; if it is near zero, default to +1.
        const float lat = (*active)[0]; // strafe in HMD-local frame
        const float fwd_sign = (std::fabs(lat) > 0.05f && std::fabs(lat) > std::fabs((*active)[1]) * 2.0f)
                                   ? 0.0f
                                   : 1.0f; // 1.0 = forward in HMD-local
        target = {lat * 0.5f * slow_walk, fwd_sign * slow_walk};
        filtered_[0] += a * (target[0] - filtered_[0]);
        filtered_[1] += a * (target[1] - filtered_[1]);
    }
    else if (!phase_active_ && active_speed < walk_speed_threshold_) {
        // No phase activity and no velocity: decay the filter toward zero.
        filtered_[0] *= 0.92f;
        filtered_[1] *= 0.92f;
    }

    left_was_up_ = left_up_now;
    right_was_up_ = right_up_now;
    return {filtered_, is_run, is_jump};
}

} // namespace LegMotion