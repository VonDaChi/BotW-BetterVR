import math
import unittest

from leg_motion_lab import LegMotionAnalyzer, Sample


def sample(t, left_pos=(0.0, 0.1, 0.0), right_pos=(0.2, 0.1, 0.0), left_vel=(0.0, 0.0, 0.0), right_vel=(0.0, 0.0, 0.0)):
    return Sample(t, 0.0, left_pos, right_pos, left_vel, right_vel)


class LegMotionAnalyzerTests(unittest.TestCase):
    def setUp(self):
        self.analyzer = LegMotionAnalyzer(smoothing_alpha=1.0)
        self.analyzer.calibrate([sample(0.0), sample(0.1)])

    def test_static_is_not_walk_or_run(self):
        result = self.analyzer.update(sample(1.0))
        self.assertEqual(result.walk_vector, (0.0, 0.0))
        self.assertFalse(result.is_run)
        self.assertFalse(result.is_jump)

    def test_backward_impulse_produces_forward_walk_vector(self):
        result = self.analyzer.update(sample(1.0, left_vel=(0.0, 0.0, 0.18)))
        self.assertGreater(result.walk_vector[1], 0.0)

    def test_fast_impulse_is_run(self):
        result = self.analyzer.update(sample(1.0, left_vel=(0.0, 0.0, 0.35)))
        self.assertTrue(result.is_run)

    def test_jump_is_one_shot_with_cooldown(self):
        self.analyzer.update(sample(1.0))
        first = self.analyzer.update(sample(1.05, left_pos=(0.0, 0.18, 0.0), right_pos=(0.2, 0.18, 0.0)))
        second = self.analyzer.update(sample(1.10, left_pos=(0.0, 0.20, 0.0), right_pos=(0.2, 0.20, 0.0)))
        after_cooldown = self.analyzer.update(sample(1.50, left_pos=(0.0, 0.22, 0.0), right_pos=(0.2, 0.22, 0.0)))
        self.assertTrue(first.is_jump)
        self.assertFalse(second.is_jump)
        self.assertFalse(after_cooldown.is_jump)

    def test_slow_walk_phase_detection(self):
        """Phase detector should synthesise walk vector for in-place slow steps.

        Simulates alternating left/right foot Y movement (heel strike pattern)
        with upward velocity > up_velocity_threshold (0.05 m/s), but zero
        horizontal velocity so the velocity-based path stays silent.
        """
        analyzer = LegMotionAnalyzer(smoothing_alpha=0.5)
        analyzer.calibrate([sample(0.0, left_pos=(0.0, 0.1, 0.0), right_pos=(0.0, 0.1, 0.0))])
        dt = 0.05
        base_y = 0.1
        step_height = 0.02
        # Simulate 10 steps: left foot lifts (0.05s up, 0.05s down), then right
        # Each step cycle is ~0.4s (2 steps = 1 gait cycle)
        # Use a sinusoidal Y pattern that triggers heel strikes with upward speed > 0.05
        walk_vector_detected = False
        prev_left_up = False
        prev_right_up = False
        for i in range(80):
            t = i * dt
            # Left foot: sin wave, period = 0.8s (2 steps per foot)
            left_y = base_y + step_height * math.sin(2 * math.pi * t / 0.8)
            right_y = base_y + step_height * math.sin(2 * math.pi * (t + 0.4) / 0.8)  # 180 degrees out of phase
            s = sample(t, left_pos=(0.0, left_y, 0.0), right_pos=(0.0, right_y, 0.0))
            result = analyzer.update(s)
            if result.walk_vector[1] > 0.01:  # forward motion detected
                walk_vector_detected = True
        self.assertTrue(walk_vector_detected, "Phase detector should produce forward walk signal for slow alternating steps")

    def test_static_no_false_phase_trigger(self):
        """Standing still should not trigger phase detection (no false walk)."""
        analyzer = LegMotionAnalyzer(smoothing_alpha=0.5)
        # Small noise around stand height
        base_y = 0.1
        for i in range(100):
            t = i * 0.05
            noise = 0.0001 * math.sin(t * 7.0)  # tiny noise
            s = sample(t, left_pos=(0.0, base_y + noise, 0.0), right_pos=(0.0, base_y + noise, 0.0))
            result = analyzer.update(s)
            self.assertEqual(result.walk_vector[1], 0.0, f"Frame {i}: no walk expected during standing")


if __name__ == "__main__":
    unittest.main()
