"""P2-B leg-motion recorder analysis prototype.

Consumes BetterVR_legrec_*.csv files produced by the P2-A recorder.
The current OpenVR runtime reports zero linear velocity for the foot poses,
so this prototype deliberately reconstructs velocity from position deltas.

Usage:
    python tools/leg_motion_lab.py Cemu/BetterVR_legrec_YYYYMMDD_HHMMSS.csv
"""

from __future__ import annotations

import argparse
import csv
import math
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Sample:
    t: float
    hmd_yaw: float
    left_pos: tuple[float, float, float]
    right_pos: tuple[float, float, float]
    left_vel: tuple[float, float, float]
    right_vel: tuple[float, float, float]


def _float(row: dict[str, str], key: str) -> float | None:
    value = row[key]
    if value.lower() == "nan":
        return None
    return float(value)


def load_samples(path: Path) -> list[Sample]:
    samples: list[Sample] = []
    with path.open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            values = {
                key: _float(row, key)
                for key in (
                    "t", "hmd_yaw",
                    "fl_x", "fl_y", "fl_z", "fr_x", "fr_y", "fr_z",
                )
            }
            if any(value is None for value in values.values()):
                continue
            samples.append(Sample(
                t=values["t"],
                hmd_yaw=values["hmd_yaw"],
                left_pos=(values["fl_x"], values["fl_y"], values["fl_z"]),
                right_pos=(values["fr_x"], values["fr_y"], values["fr_z"]),
                left_vel=(0.0, 0.0, 0.0),
                right_vel=(0.0, 0.0, 0.0),
            ))
    return reconstruct_velocities(samples)


def reconstruct_velocities(samples: list[Sample]) -> list[Sample]:
    if not samples:
        return samples
    output = [Sample(s.t, s.hmd_yaw, s.left_pos, s.right_pos, s.left_vel, s.right_vel)
              for s in samples]
    for i in range(1, len(output)):
        dt = output[i].t - output[i - 1].t
        if dt <= 0.0 or dt > 0.25:
            continue
        left = tuple((output[i].left_pos[j] - output[i - 1].left_pos[j]) / dt for j in range(3))
        right = tuple((output[i].right_pos[j] - output[i - 1].right_pos[j]) / dt for j in range(3))
        output[i].left_vel = left
        output[i].right_vel = right
    return output


def horizontal_speed(velocity: tuple[float, float, float]) -> float:
    return math.hypot(velocity[0], velocity[2])


def yaw_delta(a: float, b: float) -> float:
    return (b - a + math.pi) % (2.0 * math.pi) - math.pi


@dataclass
class MotionResult:
    walk_vector: tuple[float, float]
    is_run: bool
    is_jump: bool


class LegMotionAnalyzer:
    """Small, deterministic P2-B prototype.

    The analyzer consumes one frame at a time. Foot velocity is reconstructed before
    this class is called. Walk direction is derived from the faster foot's backward
    impulse in the HMD-local frame; averaging both feet would cancel during walking.
    """

    def __init__(
        self,
        walk_speed_threshold: float = 0.12,
        run_speed_threshold: float = 0.30,
        jump_lift_threshold: float = 0.055,
        jump_upward_speed_threshold: float = 0.30,
        deadzone: float = 0.08,
        smoothing_alpha: float = 0.22,
        jump_cooldown_s: float = 0.40,
    ) -> None:
        self.walk_speed_threshold = walk_speed_threshold
        self.run_speed_threshold = run_speed_threshold
        self.jump_lift_threshold = jump_lift_threshold
        self.jump_upward_speed_threshold = jump_upward_speed_threshold
        self.deadzone = deadzone
        self.smoothing_alpha = smoothing_alpha
        self.jump_cooldown_s = jump_cooldown_s
        self.stand_foot_y = 0.0
        self._last_t: float | None = None
        self._last_left_y: float | None = None
        self._last_right_y: float | None = None
        self._last_jump_t = -math.inf
        self._jump_armed = False
        self._filtered = [0.0, 0.0]

        # Phase-based slow-walk detection (P5). Tracks the alternating heel-strike
        # pattern of left/right feet so walking at < 0.1 m/s is still detected.
        # A minimum upward velocity gates "moving up" to reject foot-Y noise
        # during standing (std ~0.0002 m produces 150+ false heel strikes in 30s).
        self.up_velocity_threshold_ = 0.05
        self._left_phase = 0.0
        self._right_phase = 0.0
        self._left_was_up = False
        self._right_was_up = False
        self._step_period_s = 0.6
        self._last_left_hs_t = 0.0
        self._last_right_hs_t = 0.0
        self._phase_step_count = 0
        self._phase_active = False
        self._last_activity_t = 0.0
        self._activity_timeout_s = 1.5
        self.up_velocity_threshold_ = 0.05  # minimum upward velocity to count as a real step

    def calibrate(self, samples: list[Sample]) -> None:
        if not samples:
            raise ValueError("calibration requires at least one sample")
        self.stand_foot_y = sum(
            (sample.left_pos[1] + sample.right_pos[1]) * 0.5 for sample in samples
        ) / len(samples)
        self._jump_armed = True  # arm jump detection only after calibration baseline is established

    @staticmethod
    def _to_hmd_local(velocity: tuple[float, float, float], yaw: float) -> tuple[float, float]:
        """Return (right, forward), where forward is the HMD's facing direction."""
        right = (math.cos(yaw), 0.0, math.sin(yaw))
        forward = (math.sin(yaw), 0.0, -math.cos(yaw))
        return (
            velocity[0] * right[0] + velocity[2] * right[2],
            velocity[0] * forward[0] + velocity[2] * forward[2],
        )

    def update(self, sample: Sample) -> MotionResult:
        left_local = self._to_hmd_local(sample.left_vel, sample.hmd_yaw)
        right_local = self._to_hmd_local(sample.right_vel, sample.hmd_yaw)
        left_speed = math.hypot(*left_local)
        right_speed = math.hypot(*right_local)

        # During an in-place step one foot moves forward while the other pushes back.
        # Select the stronger foot and only treat its backward impulse as locomotion.
        active = left_local if left_speed >= right_speed else right_local
        active_speed = max(left_speed, right_speed)
        backward = max(0.0, -active[1])
        if active_speed < self.walk_speed_threshold:
            target = [0.0, 0.0]
        else:
            target = [active[0], backward]
            magnitude = math.hypot(*target)
            if magnitude > 0.0:
                scale = min(1.0, magnitude / self.run_speed_threshold)
                target = [target[0] * scale, target[1] * scale]
            if math.hypot(*target) < self.deadzone:
                target = [0.0, 0.0]

        a = self.smoothing_alpha
        self._filtered[0] += a * (target[0] - self._filtered[0])
        self._filtered[1] += a * (target[1] - self._filtered[1])
        walk_vector = (self._filtered[0], self._filtered[1])

        run_metric = max(left_speed, right_speed)
        is_run = run_metric >= self.run_speed_threshold

        dt = 0.0 if self._last_t is None else max(0.0, sample.t - self._last_t)
        left_up = 0.0 if self._last_left_y is None or dt <= 0.0 else (sample.left_pos[1] - self._last_left_y) / dt
        right_up = 0.0 if self._last_right_y is None or dt <= 0.0 else (sample.right_pos[1] - self._last_right_y) / dt
        # Both feet must rise together: take the LOWER foot's lift so stepping on
        # one foot (walk/run) can never satisfy the gate. Hysteresis re-arms only
        # after both feet have clearly returned to the floor (landed).
        lift_both = min(sample.left_pos[1], sample.right_pos[1]) - self.stand_foot_y
        if lift_both < self.jump_lift_threshold * 0.5:
            self._jump_armed = True
        jump_candidate = (
            lift_both >= self.jump_lift_threshold
            and min(left_up, right_up) >= self.jump_upward_speed_threshold
        )
        is_jump = (
            jump_candidate
            and self._jump_armed
            and sample.t - self._last_jump_t >= self.jump_cooldown_s
        )
        if is_jump:
            self._last_jump_t = sample.t
            self._jump_armed = False

        # --- Phase-based slow-walk detection (P5) ---
        # When walk speed is below walk_speed_threshold, the velocity-based
        # detector falls silent. But even a slow walk has an alternating
        # heel-strike pattern. Track up/down phase of each foot; a heel strike
        # is when a foot that was rising suddenly stops. Two alternating heel
        # strikes activate the phase detector.
        k_phase_step = 0.02
        left_up_now = left_up > self.up_velocity_threshold_
        right_up_now = right_up > self.up_velocity_threshold_

        if left_up_now and not self._left_was_up:
            self._left_phase = 0.0
        if right_up_now and not self._right_was_up:
            self._right_phase = 0.0
        if left_up_now:
            self._left_phase = min(1.0, self._left_phase + k_phase_step)
        if right_up_now:
            self._right_phase = min(1.0, self._right_phase + k_phase_step)

        left_heel_strike = self._left_was_up and not left_up_now
        right_heel_strike = self._right_was_up and not right_up_now
        has_activity = left_heel_strike or right_heel_strike

        # Reset the phase detector if no activity is detected for a while
        # (e.g., after upper-body waving moves the trackers). This prevents
        # phase_active_ from persisting after the user stops walking.
        if has_activity:
            self._last_activity_t = sample.t
        elif self._phase_active and (sample.t - self._last_activity_t) > self._activity_timeout_s:
            self._phase_active = False
            self._phase_step_count = 0
            self._last_activity_t = sample.t

        if left_heel_strike:
            self._phase_step_count += 1
            if self._phase_step_count >= 2 and not self._phase_active:
                self._phase_active = True
        if right_heel_strike:
            self._phase_step_count += 1
            if self._phase_step_count >= 2 and not self._phase_active:
                self._phase_active = True

        # Estimate stride period from time between same-foot heel strikes.
        if left_heel_strike:
            if self._last_left_hs_t > 0.0:
                self._step_period_s = max(0.3, min(1.2, sample.t - self._last_left_hs_t))
            self._last_left_hs_t = sample.t
        if right_heel_strike:
            if self._last_right_hs_t > 0.0:
                self._step_period_s = max(0.3, min(1.2, sample.t - self._last_right_hs_t))
            self._last_right_hs_t = sample.t

        # Synthesise slow-walk vector when phase is active but velocity is
        # silent. Forward direction comes from the active foot's lateral
        # component (strafe cue); if near zero, default forward (+y).
        if self._phase_active and active_speed < self.walk_speed_threshold and not is_run and not is_jump:
            assumed_period = self._step_period_s if self._step_period_s > 0.0 else 0.8
            cadence_score = min(1.0, assumed_period / 0.6)
            slow_walk = cadence_score * 0.35
            lat = active[0]
            fwd_sign = 0.0 if (abs(lat) > 0.05 and abs(lat) > abs(active[1]) * 2.0) else 1.0
            target = [lat * 0.5 * slow_walk, fwd_sign * slow_walk]
            self._filtered[0] += a * (target[0] - self._filtered[0])
            self._filtered[1] += a * (target[1] - self._filtered[1])
        elif not self._phase_active and active_speed < self.walk_speed_threshold:
            # No phase activity and no velocity: decay the filter toward zero.
            self._filtered[0] *= 0.92
            self._filtered[1] *= 0.92

        self._last_t = sample.t
        self._last_left_y = sample.left_pos[1]
        self._last_right_y = sample.right_pos[1]
        self._left_was_up = left_up_now
        self._right_was_up = right_up_now
        walk_vector = (self._filtered[0], self._filtered[1])
        return MotionResult(walk_vector, is_run, is_jump)


@dataclass
class Segment:
    label: str       # "still" | "walk" | "run" | "jump"
    start_t: float
    end_t: float
    n_frames: int


def auto_segment(samples: list[Sample],
                 results: "list[MotionResult] | None" = None) -> list[Segment]:
    """Automatically segment the recording using the analyzer's own outputs.

    `results` should come from `[analyzer.update(s) for s in samples]` so that
    labels are consistent with C++ behavior (smoothed walk_vector, deadzone
    gating, run/jump detection). If `results` is None, falls back to raw
    feature thresholds (not recommended).

    Args:
        samples: list of Sample with velocity already reconstructed.
        results: optional list of MotionResult from analyzer.update().
    """
    if not samples:
        return []

    MIN_DUR = 2.0
    WINDOW = 2.0  # 2 s voting window for stable labels

    if results is None:
        raise ValueError("auto_segment requires pre-computed results; pass analyzer output")

    n = len(results)
    dt = samples[1].t - samples[0].t if len(samples) > 1 else 0.02
    win_n = max(1, int(round(WINDOW / dt)))

    # Per-frame raw label
    raw_lbl: list[str] = []
    for r in results:
        if r.is_jump: raw_lbl.append("jump")
        elif r.is_run: raw_lbl.append("run")
        elif math.hypot(*r.walk_vector) > 0.08: raw_lbl.append("walk")
        else: raw_lbl.append("still")

    # Smooth via rolling majority vote
    from collections import Counter as _Counter
    rank = {"still": 0, "walk": 1, "run": 2, "jump": 3}
    smooth: list[str] = ["" for _ in range(n)]
    for i in range(n):
        lo = max(0, i - win_n + 1)
        votes = _Counter(raw_lbl[lo:i + 1])
        best = max(votes.items(), key=lambda kv: (kv[1], rank.get(kv[0], 0)))
        smooth[i] = best[0]

    # Merge walk/run into "move" for segmentation (eliminates jitter)
    det_lbl: list[str] = []
    for i in range(n):
        if smooth[i] in ("walk", "run"):
            lo = max(0, i - win_n + 1)
            wdet = [l for l in raw_lbl[lo:i + 1] if l in ("walk", "run")]
            det_lbl.append(_Counter(wdet).most_common(1)[0][0] if wdet else smooth[i])
        else:
            det_lbl.append(smooth[i])

    raw: list[tuple[str, int, int]] = [("?", 0, 0)]
    i = 0
    while i < n:
        start = i
        lbl = "move" if smooth[i] in ("walk", "run") else smooth[i]
        while i < n and ("move" if smooth[i] in ("walk", "run") else smooth[i]) == lbl:
            i += 1
        raw.append((lbl, start, i))

    # Final: deduplicate still < MIN_DUR + assign majority detailed label
    segments: list[Segment] = []
    for label, si, ei in raw[1:]:
        dur = samples[ei - 1].t - samples[si].t
        if label == "still" and dur < MIN_DUR and segments:
            segments[-1].end_t = samples[ei - 1].t
            segments[-1].n_frames += ei - si
            continue
        if label == "move":
            dets = [det_lbl[j] for j in range(si, ei)]
            final_label = _Counter(dets).most_common(1)[0][0] if dets else "walk"
        else:
            final_label = label
        segments.append(Segment(label=final_label, start_t=samples[si].t,
                                end_t=samples[ei - 1].t, n_frames=ei - si))

    if segments and segments[-1].label == "still" and (segments[-1].end_t - segments[-1].start_t) < MIN_DUR and len(segments) > 1:
        segments[-2].end_t = segments[-1].end_t
        segments[-2].n_frames += segments[-1].n_frames
        segments.pop()

    # Post-merge: adjacent same-label segments (created by still absorption)
    merged_segs: list[Segment] = []
    for seg in segments:
        if merged_segs and merged_segs[-1].label == seg.label:
            merged_segs[-1].end_t = seg.end_t
            merged_segs[-1].n_frames += seg.n_frames
        else:
            merged_segs.append(seg)

    return merged_segs


def print_report(samples: list[Sample], path: Path) -> None:
    if len(samples) < 2:
        raise ValueError("CSV contains fewer than two valid samples")
    duration = samples[-1].t - samples[0].t
    intervals = [b.t - a.t for a, b in zip(samples, samples[1:]) if b.t > a.t]
    median_dt = sorted(intervals)[len(intervals) // 2]
    t0 = samples[0].t
    print(f"file={path}")
    print(f"samples={len(samples)} duration_s={duration:.2f} median_hz={1.0 / median_dt:.2f}")
    for name, positions, velocities in (
        ("left", [s.left_pos for s in samples], [s.left_vel for s in samples]),
        ("right", [s.right_pos for s in samples], [s.right_vel for s in samples]),
    ):
        speed = [horizontal_speed(v) for v in velocities]
        y = [p[1] for p in positions]
        print(
            f"{name}: y_min={min(y):.3f} y_max={max(y):.3f} "
            f"y_range={max(y) - min(y):.3f} "
            f"diff_hspeed_max={max(speed):.3f} "
            f"diff_hspeed_mean={sum(speed) / len(speed):.3f}"
        )
    yaw_changes = [abs(yaw_delta(a.hmd_yaw, b.hmd_yaw)) for a, b in zip(samples, samples[1:])]
    print(f"hmd_yaw_step_max_rad={max(yaw_changes):.3f}")

    # HMD Y statistics (useful for detecting crouch-like events)
    hmd_y_all = [s.hmd_yaw for s in samples]  # placeholder; actual hmd_y needs parsing
    # (HMD Y is not stored in Sample; we compute it from position later)

    analyzer = LegMotionAnalyzer()
    analyzer.calibrate([sample for sample in samples if sample.t <= t0 + 30.0])
    results = [analyzer.update(sample) for sample in samples]
    run_frames = sum(result.is_run for result in results)
    jump_events = sum(result.is_jump for result in results)
    walk_frames = sum(math.hypot(*result.walk_vector) > analyzer.deadzone for result in results)
    print(f"prototype_events=walk_frames:{walk_frames} run_frames:{run_frames} jump_events:{jump_events}")
    print("native_velocity_note=CSV fl_v*/fr_v* are zero; use reconstructed position velocity")

    # --- Automatic segment analysis (uses analyzer outputs for labels) ---
    segments = auto_segment(samples, results=results)
    print(f"auto_segmented_segments={len(segments)}")
    print("segments_auto(name start_s end_s n_frames walk_f run_f jump_ev walk_ratio):")
    for seg in segments:
        bucket_samples = [s for s in samples if seg.start_t <= s.t <= seg.end_t]
        bucket_results = [r for r, s in zip(results, samples) if seg.start_t <= s.t <= seg.end_t]
        if not bucket_samples:
            continue
        w = sum(math.hypot(*r.walk_vector) > analyzer.deadzone for r in bucket_results)
        rn = sum(r.is_run for r in bucket_results)
        j = sum(r.is_jump for r in bucket_results)
        print(f"  {seg.label}: start={seg.start_t:.1f} end={seg.end_t:.1f} n={seg.n_frames} walk_f={w} run_f={rn} jump_ev={j} walk_ratio={w/len(bucket_results):.2f}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    args = parser.parse_args()
    samples = load_samples(args.csv)
    print_report(samples, args.csv)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
