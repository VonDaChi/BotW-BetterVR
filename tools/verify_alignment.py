"""P2-E numerical alignment: Python reference vs C++ leg_motion.cpp.

Mirrors the C++ code line-by-line using plain Python floats. Compares
walk_vector (error < 1e-4) and is_run/is_jump (exact match).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "tools"))

from leg_motion_lab import load_samples, LegMotionAnalyzer as PythonAnalyzer


class CppMirrorAnalyzer:
    """Drop-in mirror of src/hooking/leg_motion.cpp LegMotionAnalyzer."""

    def __init__(self):
        self.walk_speed_threshold_ = 0.12
        self.run_speed_threshold_ = 0.30
        self.jump_lift_threshold_ = 0.055
        self.jump_upward_speed_threshold_ = 0.30
        self.deadzone_ = 0.08
        self.smoothing_alpha_ = 0.22
        self.jump_cooldown_s_ = 0.40
        self.up_velocity_threshold_ = 0.05

        self.stand_foot_y_ = 0.0
        self.last_t_ = 0.0
        self.last_left_y_ = 0.0
        self.last_right_y_ = 0.0
        self.last_jump_t_ = -math.inf
        self.jump_armed_ = False
        self.filtered_ = [0.0, 0.0]

        self.left_phase_ = 0.0
        self.right_phase_ = 0.0
        self.left_was_up_ = False
        self.right_was_up_ = False
        self.step_period_s_ = 0.6
        self.last_left_hs_t_ = 0.0
        self.last_right_hs_t_ = 0.0
        self.phase_step_count_ = 0
        self.phase_active_ = False
        self.last_activity_t_ = 0.0
        self.activity_timeout_s_ = 1.5

    def calibrate(self, samples):
        if not samples:
            return
        acc = 0.0
        for s in samples:
            acc += (s.left_pos[1] + s.right_pos[1]) * 0.5
        self.stand_foot_y_ = acc / len(samples)
        self.jump_armed_ = True

    @staticmethod
    def to_hmd_local_2d(velocity, yaw):
        cos_y = math.cos(yaw)
        sin_y = math.sin(yaw)
        right = velocity[0] * cos_y + velocity[2] * sin_y
        forward = velocity[0] * sin_y - velocity[2] * cos_y
        return [right, forward]

    def update(self, sample):
        left_local = self.to_hmd_local_2d(sample.left_vel, sample.hmd_yaw)
        right_local = self.to_hmd_local_2d(sample.right_vel, sample.hmd_yaw)
        left_speed = math.hypot(left_local[0], left_local[1])
        right_speed = math.hypot(right_local[0], right_local[1])

        active = left_local if left_speed >= right_speed else right_local
        active_speed = max(left_speed, right_speed)
        backward = max(0.0, -active[1])
        target = [0.0, 0.0]
        if active_speed >= self.walk_speed_threshold_:
            target = [active[0], backward]
            magnitude = math.hypot(target[0], target[1])
            if magnitude > 0.0:
                scale = min(1.0, magnitude / self.run_speed_threshold_)
                target[0] *= scale
                target[1] *= scale
            if math.hypot(target[0], target[1]) < self.deadzone_:
                target = [0.0, 0.0]

        a = self.smoothing_alpha_
        self.filtered_[0] += a * (target[0] - self.filtered_[0])
        self.filtered_[1] += a * (target[1] - self.filtered_[1])

        run_metric = max(left_speed, right_speed)
        is_run = run_metric >= self.run_speed_threshold_

        dt = 0.0 if self.last_t_ <= 0.0 else max(0.0, sample.t - self.last_t_)
        left_up = 0.0 if dt <= 0.0 else (sample.left_pos[1] - self.last_left_y_) / dt
        right_up = 0.0 if dt <= 0.0 else (sample.right_pos[1] - self.last_right_y_) / dt
        lift_both = min(sample.left_pos[1], sample.right_pos[1]) - self.stand_foot_y_
        if lift_both < self.jump_lift_threshold_ * 0.5:
            self.jump_armed_ = True
        jump_candidate = (
            lift_both >= self.jump_lift_threshold_
            and min(left_up, right_up) >= self.jump_upward_speed_threshold_
        )
        is_jump = (
            jump_candidate
            and self.jump_armed_
            and sample.t - self.last_jump_t_ >= self.jump_cooldown_s_
        )
        if is_jump:
            self.last_jump_t_ = sample.t
            self.jump_armed_ = False

        # --- Phase-based slow-walk (P5) ---
        left_up_now = left_up > self.up_velocity_threshold_
        right_up_now = right_up > self.up_velocity_threshold_
        k_phase_step = 0.02

        if left_up_now and not self.left_was_up_:
            self.left_phase_ = 0.0
        if right_up_now and not self.right_was_up_:
            self.right_phase_ = 0.0
        if left_up_now:
            self.left_phase_ = min(1.0, self.left_phase_ + k_phase_step)
        if right_up_now:
            self.right_phase_ = min(1.0, self.right_phase_ + k_phase_step)

        left_heel_strike = self.left_was_up_ and not left_up_now
        right_heel_strike = self.right_was_up_ and not right_up_now
        has_activity = left_heel_strike or right_heel_strike

        if has_activity:
            self.last_activity_t_ = sample.t
        elif self.phase_active_ and (sample.t - self.last_activity_t_) > self.activity_timeout_s_:
            self.phase_active_ = False
            self.phase_step_count_ = 0
            self.last_activity_t_ = sample.t

        if left_heel_strike:
            self.phase_step_count_ += 1
            if self.phase_step_count_ >= 2 and not self.phase_active_:
                self.phase_active_ = True
        if right_heel_strike:
            self.phase_step_count_ += 1
            if self.phase_step_count_ >= 2 and not self.phase_active_:
                self.phase_active_ = True

        if left_heel_strike:
            if self.last_left_hs_t_ > 0.0:
                self.step_period_s_ = max(0.3, min(1.2, sample.t - self.last_left_hs_t_))
            self.last_left_hs_t_ = sample.t
        if right_heel_strike:
            if self.last_right_hs_t_ > 0.0:
                self.step_period_s_ = max(0.3, min(1.2, sample.t - self.last_right_hs_t_))
            self.last_right_hs_t_ = sample.t

        if self.phase_active_ and active_speed < self.walk_speed_threshold_ and not is_run and not is_jump:
            assumed_period = self.step_period_s_ if self.step_period_s_ > 0.0 else 0.8
            cadence_score = min(1.0, assumed_period / 0.6)
            slow_walk = cadence_score * 0.35
            lat = active[0]
            fwd_sign = 0.0 if (abs(lat) > 0.05 and abs(lat) > abs(active[1]) * 2.0) else 1.0
            target = [lat * 0.5 * slow_walk, fwd_sign * slow_walk]
            self.filtered_[0] += a * (target[0] - self.filtered_[0])
            self.filtered_[1] += a * (target[1] - self.filtered_[1])
        elif not self.phase_active_ and active_speed < self.walk_speed_threshold_:
            self.filtered_[0] *= 0.92
            self.filtered_[1] *= 0.92

        self.last_t_ = sample.t
        self.last_left_y_ = sample.left_pos[1]
        self.last_right_y_ = sample.right_pos[1]
        self.left_was_up_ = left_up_now
        self.right_was_up_ = right_up_now

        return (self.filtered_[0], self.filtered_[1]), is_run, is_jump


def main():
    csv_path = Path("C:/Users/win/Downloads/BotW-BetterVR-0.9.22/Cemu/BetterVR_legrec_20260911_203910.csv")
    samples = load_samples(csv_path)

    # Python analyzer (current production code)
    py_analyzer = PythonAnalyzer()
    py_analyzer.calibrate([s for s in samples if s.t <= samples[0].t + 30.0])
    py_results = [py_analyzer.update(s) for s in samples]

    # C++ mirror analyzer
    cpp_analyzer = CppMirrorAnalyzer()
    cpp_analyzer.calibrate([s for s in samples if s.t <= samples[0].t + 30.0])
    cpp_results = [cpp_analyzer.update(s) for s in samples]

    assert len(py_results) == len(cpp_results), "frame count mismatch"

    max_walk_err = 0.0
    walk_mismatches = 0
    run_mismatches = 0
    jump_mismatches = 0
    bad_frames = []

    for i, (pr, cr) in enumerate(zip(py_results, cpp_results)):
        px, py = pr.walk_vector
        cx, cy = cr[0]
        err = math.hypot(px - cx, py - cy)
        if err > max_walk_err:
            max_walk_err = err
        if err > 1e-4:
            walk_mismatches += 1
            if len(bad_frames) < 10:
                bad_frames.append((i, samples[i].t, px, py, cx, cy, err))
        if pr.is_run != cr[1]:
            run_mismatches += 1
        if pr.is_jump != cr[2]:
            jump_mismatches += 1

    print(f"frames={len(py_results)}")
    print(f"walk_vector max_err={max_walk_err:.6e} mismatches={walk_mismatches}")
    print(f"is_run  mismatches={run_mismatches}")
    print(f"is_jump mismatches={jump_mismatches}")

    if bad_frames:
        print("\nFirst walk_vector mismatches:")
        for i, t, px, py, cx, cy, err in bad_frames:
            print(f"  frame {i} t={t:.4f} py=({px:.6f},{py:.6f}) cpp=({cx:.6f},{cy:.6f}) err={err:.6e}")

    if run_mismatches:
        print("\nis_run mismatches:")
        for i, (pr, cr) in enumerate(zip(py_results, cpp_results)):
            if pr.is_run != cr[1]:
                print(f"  frame {i} t={samples[i].t:.4f} py={pr.is_run} cpp={cr[1]}")

    if jump_mismatches:
        print("\nis_jump mismatches:")
        for i, (pr, cr) in enumerate(zip(py_results, cpp_results)):
            if pr.is_jump != cr[2]:
                print(f"  frame {i} t={samples[i].t:.4f} py={pr.is_jump} cpp={cr[2]}")

    ok = (max_walk_err < 1e-4 and run_mismatches == 0 and jump_mismatches == 0)
    print(f"\nVERDICT: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
