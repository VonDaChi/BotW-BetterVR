"""P2-B-2 threshold calibration.

Feeds the 9/11 CSV through the analyzer + auto_segment and prints
per-segment statistics so that recommended thresholds can be
selected for the C++ implementation.

Also prints a summary table:
  - Per segment: label, start_s, end_s, avg|max hspeed, max run_metric,
    min lift_both, max up_speed, avg walk_vec magnitude.
  - Distribution of walk_vector magnitude for each analyzer label
    (still/walk/run/jump) across the whole recording.

Output is plain text suitable for copy-paste into a design doc.
"""
from __future__ import annotations

import math
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from leg_motion_lab import (
    Segment,
    load_samples,
    LegMotionAnalyzer,
    auto_segment,
    horizontal_speed,
)


def percentile(sorted_values: list[float], pct: float) -> float:
    if not sorted_values:
        return 0.0
    k = (len(sorted_values) - 1) * (pct / 100.0)
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return sorted_values[int(k)]
    return sorted_values[f] * (c - k) + sorted_values[c] * (k - f)


def collect_per_frame(samples, results):
    """Return per-frame dicts for statistics."""
    rows = []
    for i, (s, r) in enumerate(zip(samples, results)):
        left_local = LegMotionAnalyzer._to_hmd_local(s.left_vel, s.hmd_yaw)
        right_local = LegMotionAnalyzer._to_hmd_local(s.right_vel, s.hmd_yaw)
        left_speed = math.hypot(*left_local)
        right_speed = math.hypot(*right_local)
        run_metric = max(left_speed, right_speed)
        active = left_local if left_speed >= right_speed else right_local
        backward = max(0.0, -active[1])
        mag = math.hypot(*r.walk_vector)
        rows.append({
            "t": s.t,
            "left_speed": left_speed,
            "right_speed": right_speed,
            "run_metric": run_metric,
            "backward": backward,
            "walk_vec_mag": mag,
            "is_walk": mag > 0.08,
            "is_run": r.is_run,
            "is_jump": r.is_jump,
            "left_y": s.left_pos[1],
            "right_y": s.right_pos[1],
        })
    return rows


def stats_for_slice(rows, t0, t1):
    sl = [r for r in rows if t0 <= r["t"] <= t1]
    if not sl:
        return None
    hs = [max(r["left_speed"], r["right_speed"]) for r in sl]
    wmag = [r["walk_vec_mag"] for r in sl]
    lifts = [min(r["left_y"], r["right_y"]) for r in sl]
    ups = []
    for i in range(1, len(sl)):
        dt = sl[i]["t"] - sl[i - 1]["t"]
        if dt > 0:
            ups.append((sl[i]["left_y"] - sl[i - 1]["left_y"]) / dt)
            ups.append((sl[i]["right_y"] - sl[i - 1]["right_y"]) / dt)
    return {
        "n": len(sl),
        "hspeed_max": max(hs) if hs else 0.0,
        "hspeed_p50": percentile(sorted(hs), 50) if hs else 0.0,
        "hspeed_p90": percentile(sorted(hs), 90) if hs else 0.0,
        "hspeed_p95": percentile(sorted(hs), 95) if hs else 0.0,
        "walk_vec_max": max(wmag) if wmag else 0.0,
        "walk_vec_p50": percentile(sorted(wmag), 50) if wmag else 0.0,
        "walk_vec_p95": percentile(sorted(wmag), 95) if wmag else 0.0,
        "lift_min": min(lifts) if lifts else 0.0,
        "lift_max": max(lifts) if lifts else 0.0,
        "up_speed_max": max(ups) if ups else 0.0,
        "up_speed_p90": percentile(sorted(ups), 90) if ups else 0.0,
        "up_speed_p95": percentile(sorted(ups), 95) if ups else 0.0,
        "is_jump_frames": sum(1 for r in sl if r["is_jump"]),
        "is_run_frames": sum(1 for r in sl if r["is_run"]),
        "is_walk_frames": sum(1 for r in sl if r["is_walk"]),
    }


def main() -> int:
    csv_path = Path("C:/Users/win/Downloads/BotW-BetterVR-0.9.22/Cemu/BetterVR_legrec_20260911_203910.csv")
    samples = load_samples(csv_path)
    analyzer = LegMotionAnalyzer()
    analyzer.calibrate([s for s in samples if s.t <= samples[0].t + 30.0])
    results = [analyzer.update(s) for s in samples]
    segments = auto_segment(samples, results=results)
    rows = collect_per_frame(samples, results)

    print(f"file={csv_path.name}")
    print(f"samples={len(samples)} median_hz={1.0 / (samples[1].t - samples[0].t):.2f}")
    print(f"analyzer_defaults=walk_speed={analyzer.walk_speed_threshold} "
          f"run_speed={analyzer.run_speed_threshold} "
          f"jump_lift={analyzer.jump_lift_threshold} "
          f"jump_up={analyzer.jump_upward_speed_threshold} "
          f"deadzone={analyzer.deadzone} "
          f"alpha={analyzer.smoothing_alpha}")
    print()

    # --- Segment-level detail ---
    print("=== SEGMENT STATISTICS ===")
    print(f"{'label':6s} {'start':7s} {'end':7s} {'n':4s} "
          f"{'hspeed_p50':10s} {'hspeed_p95':10s} {'hspeed_max':10s} "
          f"{'wmag_p95':9s} {'lift_max':9s} {'up_p95':7s} "
          f"{'jump_f':6s} {'run_f':5s} {'walk_f':6s}")
    print("-" * 120)
    for seg in segments:
        st = stats_for_slice(rows, seg.start_t, seg.end_t)
        if not st:
            continue
        print(
            f"{seg.label:6s} {seg.start_t:7.1f} {seg.end_t:7.1f} {st['n']:4d} "
            f"{st['hspeed_p50']:10.4f} {st['hspeed_p95']:10.4f} {st['hspeed_max']:10.4f} "
            f"{st['walk_vec_p95']:9.4f} {st['lift_max']:9.4f} {st['up_speed_p95']:7.3f} "
            f"{st['is_jump_frames']:6d} {st['is_run_frames']:5d} {st['is_walk_frames']:6d}"
        )

    print()
    print("=== PER-SEGMENT-LABEL AGGREGATE (still/walk/run/jump) ===")
    buckets: dict[str, list[dict]] = defaultdict(list)
    for seg in segments:
        st = stats_for_slice(rows, seg.start_t, seg.end_t)
        if st:
            buckets[seg.label].append(st)

    for label in ("still", "walk", "run", "jump"):
        bks = buckets.get(label, [])
        if not bks:
            continue
        all_hs_p95 = sorted([b["hspeed_p95"] for b in bks])
        all_rm = sorted([b["hspeed_max"] for b in bks])
        all_wmag_p95 = sorted([b["walk_vec_p95"] for b in bks])
        all_lift = sorted([b["lift_max"] for b in bks])
        all_up = sorted([b["up_speed_p95"] for b in bks])
        total_n = sum(b["n"] for b in bks)
        print(f"label={label} segments={len(bks)} total_frames={total_n}")
        print(f"  hspeed_p95  min={all_hs_p95[0]:.4f} p50={percentile(all_hs_p95,50):.4f} max={all_hs_p95[-1]:.4f}")
        print(f"  hspeed_max  min={all_rm[0]:.4f} p50={percentile(all_rm,50):.4f} max={all_rm[-1]:.4f}")
        print(f"  walk_vec_p95 min={all_wmag_p95[0]:.4f} p50={percentile(all_wmag_p95,50):.4f} max={all_wmag_p95[-1]:.4f}")
        print(f"  lift_max    min={all_lift[0]:.4f} p50={percentile(all_lift,50):.4f} max={all_lift[-1]:.4f}")
        print(f"  up_speed_p95 min={all_up[0]:.4f} p50={percentile(all_up,50):.4f} max={all_up[-1]:.4f}")

    print()
    print("=== ANALYZER PER-FRAME LABEL DISTRIBUTION (whole recording) ===")
    c = Counter()
    for r in rows:
        if r["is_jump"]:
            c["jump"] += 1
        elif r["is_run"]:
            c["run"] += 1
        elif r["is_walk"]:
            c["walk"] += 1
        else:
            c["still"] += 1
    for k, v in sorted(c.items(), key=lambda x: -x[1]):
        print(f"  {k}: {v} frames ({v / len(rows) * 100:.1f}%)")

    # --- Threshold recommendations ---
    still_bks = buckets.get("still", [])
    walk_bks = buckets.get("walk", [])
    run_bks = buckets.get("run", [])
    jump_bks = buckets.get("jump", [])

    print()
    print("=== RECOMMENDED THRESHOLD GUIDELINES (from 9/11 data) ===")
    if still_bks and walk_bks:
        still_p95 = percentile(sorted([b["hspeed_p95"] for b in still_bks]), 95)
        walk_p50 = percentile(sorted([b["hspeed_p50"] for b in walk_bks]), 50)
        print(f"  walk_speed_threshold > still_p95({still_p95:.4f})  → current=0.12  (margin={walk_bks[0]['hspeed_p50'] - still_p95:.4f} if positive)")
    if walk_bks and run_bks:
        walk_max = max(b["hspeed_max"] for b in walk_bks)
        run_min = min(b["hspeed_max"] for b in run_bks)
        print(f"  run_speed_threshold between walk_max({walk_max:.4f}) and run_min({run_min:.4f})  → current=0.30")
    if jump_bks:
        lift_vals = [b["lift_max"] for b in jump_bks]
        up_vals = [b["up_speed_p95"] for b in jump_bks]
        print(f"  jump_lift_threshold  jump seg lift_max min={min(lift_vals):.4f} max={max(lift_vals):.4f}  → current=0.055")
        print(f"  jump_upward_threshold jump seg up_p95 min={min(up_vals):.4f} max={max(up_vals):.4f}  → current=0.30")
    print("  deadzone=0.08 (filters out walk_vector magnitudes below motion threshold)")
    print("  smoothing_alpha=0.22 (exponential smoothing, lower=slower response)")
    print("  up_velocity_threshold_=0.05 (foot upward velocity for heel-strike detection)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
