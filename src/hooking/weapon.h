#pragma once

#include "pch.h"

#include "utils/mod_settings.h"

enum class AttackType {
    None = 0,
    Slash = 1,
    Stab = 2
};

// Pure motion record - stores pre-computed local-space values.
// No debug-specific fields; the overlay reads these directly.
struct MotionSample {
    XrTime time = 0;
    glm::fvec3 position = {};
    glm::fquat rotation = glm::identity<glm::fquat>();

    // Pre-computed local-space values (already rotated by inverse(rotation))
    glm::fvec3 localLinearVelocity = {};
    glm::fvec3 localAngularVelocity = {};
    glm::fvec3 localLinearAcceleration = {};

    // Smoothed variants (EMA-filtered)
    glm::fvec3 smoothedLinearAcceleration = {};
    float smoothedFlatAngAcc = 0.0f;
    float smoothedAngularDrift = 0.0f;

    // Per-sample state (genuinely per-sample, not debug bookkeeping)
    AttackType attackType = AttackType::None;
    float handSpeed = 0.0f;
};

struct WeaponProfile {
    // Stab detection
    float stabSpeedThreshold;             // [m/s] minimum forward velocity to maintain stab
    float stabAccThreshold;               // [m/s^2] minimum forward acceleration to start stab
    float stabLinearSteadinessThreshold;   // [unitless] min Z-component ratio of velocity direction
    float stabAngularSteadinessThreshold;  // [rad/s] max angular velocity during stab
    float stabTravelDistance;              // [m] forward travel before stab activates hitbox
    float stabAccSmoothingTimeConstant;    // [s] EMA time constant for the stab acceleration gate
    float stabVelocityThreshold;           // [m/s] sustained forward speed that also opens the stab gate

    // Slash detection
    float slashSpeedThreshold;      // [rad/s] minimum angular velocity (XY plane) to maintain slash
    float slashAccThreshold;        // [rad/s^2] minimum angular acceleration to initiate slash
    float slashVelocityThreshold;   // [rad/s] alternate slash detection via angular velocity magnitude
    float slashAccDriftThreshold;   // [rad/s^2] max angular velocity axis drift before slash fails
    float slashTravelAngle;         // [rad] minimum rotation angle before slash activates hitbox

    // Tolerance
    float maxBadDuration;                             // [s] accumulated bad-sample time before attack cancels
    XrTime goodSampleGracePeriod;                  // [ns] grace period before resetting good-sample counter
    float minGoodSwingDuration;                     // [s] accumulated good-sample time before slash locks
    float minGoodStabDuration;                      // [s] accumulated good-sample time before stab locks
    float smoothingTimeConstant;                     // [s] EMA time constant (larger = more smoothing)
    float angularDriftMinVelocity;                  // [rad/s] minimum angular velocity for drift calculation

    // Auto-calibration
    float referenceArmLength;  // [m] baseline arm length for threshold scaling

    static WeaponProfile ForSensitivity(SwingSensitivity sensitivity);
};

struct WeaponDebugSnapshot {
    WeaponType weaponType = WeaponType::LargeSword;
    AttackType lockedAttackType = AttackType::None;
    XrTime sampleTime = 0;
    bool attackActive = false;
    bool hitboxEnabled = false;
    float swingPower = 0.0f;
    float badSampleAccumTime = 0.0f;
    float maxBadDuration = 0.0f;
    float calibratedArmLength = 0.0f;

    float forwardStabSpeed = 0.0f;
    float stabSpeedThreshold = 0.0f;
    float forwardStabAcceleration = 0.0f;
    float stabAccThreshold = 0.0f;
    float stabDirectionAbsZ = 0.0f;
    float stabLinearSteadinessThreshold = 0.0f;
    float stabAngularXY = 0.0f;
    float stabAngularSteadinessThreshold = 0.0f;
    float goodStabAccumTime = 0.0f;
    float minGoodStabDuration = 0.0f;

    float slashAngularSpeed = 0.0f;
    float slashSpeedThreshold = 0.0f;
    float slashAngularAcceleration = 0.0f;
    float slashAccThreshold = 0.0f;
    float slashVelocityThreshold = 0.0f;
    float slashAngularDrift = 0.0f;
    float slashAccDriftThreshold = 0.0f;
    float goodSwingAccumTime = 0.0f;
    float minGoodSwingDuration = 0.0f;

    bool stabSpeedOk = false;
    bool stabAccelOk = false;
    bool stabDirectionOk = false;
    bool stabAngularOk = false;
    bool stabDurationOk = false;
    bool slashSpeedOk = false;
    bool slashAccelOk = false;
    bool slashVelocityOk = false;
    bool slashDriftOk = false;
    bool slashDurationOk = false;
};

// Returns a coherent set of profile values for the given sensitivity preset.
inline WeaponProfile WeaponProfile::ForSensitivity(SwingSensitivity sensitivity) {
    WeaponProfile p = {};

    // Shared defaults (Relaxed preset)
    p.stabSpeedThreshold = 0.05f;
    p.stabAccThreshold = 4.0f;
    p.stabLinearSteadinessThreshold = glm::cos(glm::pi<float>() / 3.0f); // ~60 deg cone to avoid over-penalizing wrist angle
    p.stabAngularSteadinessThreshold = 4.5f;
    p.stabTravelDistance = 0.12f;
    p.stabAccSmoothingTimeConstant = 0.050f;
    p.stabVelocityThreshold = 1.0f;

    p.slashSpeedThreshold = 1.0f;
    p.slashAccThreshold = 14.0f;
    p.slashVelocityThreshold = 5.0f;
    p.slashAccDriftThreshold = 10.0f;
    p.slashTravelAngle = glm::pi<float>() / 6.0f; // 30 deg

    p.maxBadDuration = 0.033f;
    p.goodSampleGracePeriod = (XrTime)80e6; // 80 ms
    p.minGoodSwingDuration = 0.025f;
    p.minGoodStabDuration = 0.025f;
    p.smoothingTimeConstant = 0.030f;
    p.angularDriftMinVelocity = 0.5f;

    p.referenceArmLength = 0.55f;

    switch (sensitivity) {
        case SwingSensitivity::SWING_EASY:
            // Already set above
            break;

        case SwingSensitivity::SWING_NORMAL:
            p.stabLinearSteadinessThreshold = glm::cos(glm::pi<float>() / 4.0f); // ~45 deg cone, still stricter than relaxed
            p.slashAccThreshold = 20.0f;
            p.slashVelocityThreshold = 7.0f;
            p.slashSpeedThreshold = 1.5f;
            p.slashTravelAngle = glm::pi<float>() / 5.0f; // 36 deg
            p.maxBadDuration = 0.022f;
            p.goodSampleGracePeriod = (XrTime)40e6; // 40 ms
            p.minGoodSwingDuration = 0.040f;
            p.minGoodStabDuration = 0.030f;
            p.smoothingTimeConstant = 0.020f;
            break;
    }

    return p;
}

class WeaponMotionAnalyser {
public:
    static constexpr int MAX_SAMPLES = 90;
    static constexpr float HAND_VELOCITY_LENGTH_THRESHOLD = 2.0f;

    WeaponMotionAnalyser() = default;

    void Update(const XrSpaceLocation& handLocation, const XrSpaceVelocity& handVelocity, const glm::fmat4& headsetMtx, XrTime inputTime);

    bool IsAttacking() const { return m_attackActive; }
    bool IsHitboxEnabled() const { return m_isHitboxEnabled; }
    void SetHitboxEnabled(bool enabled) { m_isHitboxEnabled = enabled; }

    // Returns 0.0-1.0 based on peak velocity during the current attack
    float GetSwingPower() const { return m_swingPower; }

    float GetHandSpeed() const { return m_handSpeed; }

    void ResetSwing();
    void ResetStab();
    void Reset();
    void ResetIfWeaponTypeChanged(WeaponType weaponType);

    void ApplyProfile(SwingSensitivity sensitivity);

    WeaponDebugSnapshot GetDebugSnapshot() const;
    const std::array<MotionSample, MAX_SAMPLES>& GetSamples() const { return m_samples; }
    uint32_t GetLastSampleIndex() const { return m_lastSampleIdx; }

private:
    // ---- Signal processing ----
    void computeLocalSignals(const glm::fvec3& linearVelocity, const glm::fvec3& angularVelocity, const glm::fquat& rotation, float dt);
    void computeSmoothedSignals(float dt);
    float computeAngularDrift(const glm::fvec3& angularVelocity, float dt);
    void updateArmCalibration(float handToHeadDist);

    // ---- Detection pipeline ----
    void detectAttackType(const glm::fvec3& position, const glm::fquat& rotation, XrTime inputTime, float dt);
    void checkAttackSteadiness(XrTime inputTime, float dt);
    void updateAttackActivity(const glm::fquat& rotation, const glm::fvec3& position);
    void updateSwingPower();

    // ---- Profile ----
    WeaponProfile m_profile = WeaponProfile::ForSensitivity(SwingSensitivity::SWING_NORMAL);

    // ---- Sample buffer ----
    std::array<MotionSample, MAX_SAMPLES> m_samples = {};
    uint32_t m_sampleIdx = 0;
    uint32_t m_lastSampleIdx = 0;

    // ---- Previous-frame state for derivatives ----
    glm::fvec3 m_prevLocalLinVel = {};
    glm::fvec3 m_prevFlatAngVel = {};
    glm::fvec3 m_prevAngularVelocity = {};  // world-space, for drift calc
    XrTime m_prevSampleTime = 0;

    // ---- Smoothed signals (EMA-filtered) ----
    glm::fvec3 m_smoothedLinAccel = {};
    glm::fvec3 m_smoothedLinAccelSlow = {};
    float m_smoothedFlatAngAcc = 0.0f;
    float m_smoothedAngDrift = 0.0f;
    float m_smoothedFrameDt = 0.0f;

    // ---- Current-frame computed values (used across detection methods) ----
    glm::fvec3 m_localLinVel = {};
    glm::fvec3 m_localAngVel = {};
    glm::fvec3 m_localLinAccel = {};
    float m_flatAngAcc = 0.0f;
    float m_angularDrift = 0.0f;
    float m_handSpeed = 0.0f;

    // ---- Attack state machine ----
    AttackType m_lockedAttackType = AttackType::None;
    bool m_attackActive = false;
    bool m_isHitboxEnabled = false;

    float m_goodSwingAccumTime = 0.0f;
    float m_goodStabAccumTime = 0.0f;
    float m_badSampleAccumTime = 0.0f;

    XrTime m_lastGoodSwingTime = 0;
    XrTime m_lastGoodStabTime = 0;
    XrTime m_lockTime = 0;
    XrTime m_refractoryUntil = 0;

    glm::fvec3 m_lockedPosition = {};
    glm::fvec3 m_lockedAngle = {};

    // ---- Swing power tracking ----
    float m_swingPower = 0.0f;
    float m_peakAttackVelocity = 0.0f;

    // ---- Auto-calibration ----
    float m_calibratedArmLength = 0.55f;

    // ---- Weapon type ----
    WeaponType m_weaponType = WeaponType::LargeSword;

    // ---- Reference velocities for swing power normalization ----
    static constexpr float REFERENCE_SLASH_VELOCITY = 10.0f;  // rad/s for a strong slash
    static constexpr float REFERENCE_STAB_VELOCITY = 2.0f;    // m/s for a strong stab
};

inline void WeaponMotionAnalyser::ApplyProfile(SwingSensitivity sensitivity) {
    m_profile = WeaponProfile::ForSensitivity(sensitivity);
}

inline void WeaponMotionAnalyser::computeLocalSignals(
    const glm::fvec3& linearVelocity,
    const glm::fvec3& angularVelocity,
    const glm::fquat& rotation,
    float dt)
{
    const glm::fquat invRotation = glm::inverse(rotation);
    m_localLinVel = invRotation * linearVelocity;
    m_localAngVel = invRotation * angularVelocity;

    m_localLinAccel = (dt > 0.0f) ? (m_localLinVel - m_prevLocalLinVel) / dt : glm::fvec3(0.0f);

    // Angular acceleration over XY plane (removing twist around Z)
    const glm::fvec3 flatAngVel = m_localAngVel - glm::fvec3(0.0f, 0.0f, m_localAngVel.z);
    m_flatAngAcc = (dt > 0.0f) ? glm::length(flatAngVel - m_prevFlatAngVel) / dt : 0.0f;
    m_prevFlatAngVel = flatAngVel;

    m_handSpeed = glm::length(linearVelocity);
}

inline float WeaponMotionAnalyser::computeAngularDrift(const glm::fvec3& angularVelocity, float dt) {
    const float magCurr = glm::length(angularVelocity);
    const float magPrev = glm::length(m_prevAngularVelocity);

    // Guard against noise when velocities are near-zero
    if (magCurr < m_profile.angularDriftMinVelocity || magPrev < m_profile.angularDriftMinVelocity || dt <= 0.0f) {
        return 0.0f;
    }

    const float cosAngle = glm::clamp(
        glm::dot(glm::normalize(angularVelocity), glm::normalize(m_prevAngularVelocity)),
        -1.0f, 1.0f);
    return std::acos(cosAngle) / dt;
}

inline void WeaponMotionAnalyser::computeSmoothedSignals(float dt) {
    const float alpha = (dt > 0.0f) ? 1.0f - std::exp(-dt / m_profile.smoothingTimeConstant) : 0.0f;
    m_smoothedLinAccel = glm::mix(m_smoothedLinAccel, m_localLinAccel, alpha);
    m_smoothedFlatAngAcc = std::lerp(m_smoothedFlatAngAcc, m_flatAngAcc, alpha);
    m_smoothedAngDrift = std::lerp(m_smoothedAngDrift, m_angularDrift, alpha);

    const float alphaSlow = (dt > 0.0f) ? 1.0f - std::exp(-dt / m_profile.stabAccSmoothingTimeConstant) : 0.0f;
    m_smoothedLinAccelSlow = glm::mix(m_smoothedLinAccelSlow, m_localLinAccel, alphaSlow);
}

inline void WeaponMotionAnalyser::updateArmCalibration(float handToHeadDist) {
    // Slowly converge toward the upper range of observed hand distances.
    // Biased toward larger values to avoid penalizing players with shorter arms
    // who occasionally extend further.
    if (handToHeadDist > m_calibratedArmLength) {
        m_calibratedArmLength = std::lerp(m_calibratedArmLength, handToHeadDist, 0.05f);
    }
    else {
        m_calibratedArmLength = std::lerp(m_calibratedArmLength, handToHeadDist, 0.002f);
    }
}

inline void WeaponMotionAnalyser::detectAttackType(const glm::fvec3& position, const glm::fquat& rotation, XrTime inputTime, float dt) {
    if (m_lockedAttackType != AttackType::None)
        return;

    // Keeps the return-to-guard motion after an attack from immediately re-firing.
    const bool inRefractory = inputTime < m_refractoryUntil;

    // ---- Stab detection ----
    bool stabGateOk = false;
    if (glm::length2(m_localLinVel) >= 1e-8f && !inRefractory) {
        const glm::fvec3 stabDir = glm::normalize(m_localLinVel);
        const bool stabDirectionOk = std::abs(stabDir.z) > m_profile.stabLinearSteadinessThreshold;
        const bool stabAngularOk = std::abs(m_localAngVel.x) < m_profile.stabAngularSteadinessThreshold
                                && std::abs(m_localAngVel.y) < m_profile.stabAngularSteadinessThreshold;
        const bool stabImpulseOk = -m_smoothedLinAccelSlow.z > m_profile.stabAccThreshold
                                || -m_localLinVel.z > m_profile.stabVelocityThreshold;

        stabGateOk = stabDirectionOk && stabAngularOk && stabImpulseOk;
        if (stabGateOk) {
            // Latched at the streak start so the whole thrust counts toward the travel gate.
            if (m_goodStabAccumTime == 0.0f) {
                m_lockedPosition = position;
            }
            m_goodStabAccumTime += dt;
            m_lastGoodStabTime = inputTime;
            Log::print<CONTROLS>("Stab detect attack");
            if (m_goodStabAccumTime > m_profile.minGoodStabDuration) {
                m_lockedAttackType = AttackType::Stab;
                m_peakAttackVelocity = 0.0f;
                m_lockTime = inputTime;
            }
        }
        else if ((inputTime - m_lastGoodStabTime) > m_profile.goodSampleGracePeriod) {
            m_goodStabAccumTime = 0.0f;
        }
    }

    // ---- Slash detection ----
    // Two paths: angular acceleration spike OR sustained high angular velocity.
    // A stab-quality frame or a fresh stab lock keeps the slash path from claiming it.
    if (!stabGateOk && m_lockedAttackType != AttackType::Stab) {
        const float flatAngVelMag = glm::length(glm::fvec3(m_localAngVel.x, m_localAngVel.y, 0.0f));
        const bool slashByAccel = !inRefractory && m_smoothedFlatAngAcc > m_profile.slashAccThreshold;
        const bool slashByVelocity = flatAngVelMag > m_profile.slashVelocityThreshold;

        if (slashByAccel || slashByVelocity) {
            if (m_goodSwingAccumTime == 0.0f) {
                m_lockedAngle = rotation * glm::fvec3(0.0f, 0.0f, 1.0f);
            }
            m_goodSwingAccumTime += dt;
            m_lastGoodSwingTime = inputTime;
            Log::print<CONTROLS>("Slash attack detected (accel={}, vel={})", slashByAccel, slashByVelocity);
            if (m_goodSwingAccumTime > m_profile.minGoodSwingDuration) {
                m_lockedAttackType = AttackType::Slash;
                m_peakAttackVelocity = 0.0f;
                m_lockTime = inputTime;
            }
        }
        else if ((inputTime - m_lastGoodSwingTime) > m_profile.goodSampleGracePeriod) {
            m_goodSwingAccumTime = 0.0f;
        }
    }
}

inline void WeaponMotionAnalyser::checkAttackSteadiness(XrTime inputTime, float dt) {
    bool isBadSample = false;

    switch (m_lockedAttackType) {
        case AttackType::None:
            return;

        case AttackType::Stab: {
            if (glm::length2(m_localLinVel) < 1e-8f) {
                isBadSample = true;
            }
            else {
                const glm::fvec3 stabDir = glm::normalize(m_localLinVel);
                const float angXY = glm::length(glm::fvec3(m_localAngVel.x, m_localAngVel.y, 0.0f));

                isBadSample = std::abs(stabDir.z) < m_profile.stabLinearSteadinessThreshold
                           || -m_localLinVel.z < m_profile.stabSpeedThreshold
                           || angXY > m_profile.stabAngularSteadinessThreshold;
            }
            break;
        }

        case AttackType::Slash: {
            const float flatAngVelMag = glm::length(glm::fvec3(m_localAngVel.x, m_localAngVel.y, 0.0f));

            if (m_smoothedAngDrift > m_profile.slashAccDriftThreshold) {
                Log::print<CONTROLS>("[FAIL]: Drift: {}/{}", m_smoothedAngDrift, m_profile.slashAccDriftThreshold);
                isBadSample = true;
            }
            else if (flatAngVelMag < m_profile.slashSpeedThreshold) {
                Log::print<CONTROLS>("[FAIL]: Angular velocity: {}/{}", flatAngVelMag, m_profile.slashSpeedThreshold);
                isBadSample = true;
            }
            break;
        }
    }

    // Scales with frame time so a lock cannot cancel on the same samples that set it.
    const XrTime lockGrace = std::max((XrTime)33e6, (XrTime)(2.5f * m_smoothedFrameDt * 1e9f));
    if (inputTime - m_lockTime < lockGrace) {
        isBadSample = false;
    }

    // Decrement model: good frames heal, bad frames accumulate
    if (isBadSample) {
        m_badSampleAccumTime += dt;
    }
    else {
        m_badSampleAccumTime = std::max(0.0f, m_badSampleAccumTime - dt);
    }

    const float maxBadDuration = std::max(m_profile.maxBadDuration, 2.5f * m_smoothedFrameDt);
    if (m_badSampleAccumTime >= maxBadDuration) {
        m_badSampleAccumTime = 0.0f;
        if (m_attackActive) {
            m_refractoryUntil = inputTime + (XrTime)250e6;
        }
        if (m_lockedAttackType == AttackType::Slash) {
            m_goodSwingAccumTime = 0.0f;
            m_lastGoodSwingTime = 0;
        }
        else {
            m_goodStabAccumTime = 0.0f;
            m_lastGoodStabTime = 0;
        }
        m_lockedAttackType = AttackType::None;
    }
}

inline void WeaponMotionAnalyser::updateAttackActivity(const glm::fquat& rotation, const glm::fvec3& position) {
    if (m_lockedAttackType == AttackType::None) {
        m_attackActive = false;
        return;
    }

    if (m_attackActive)
        return;

    const float armScale = m_calibratedArmLength / m_profile.referenceArmLength;

    switch (m_lockedAttackType) {
        case AttackType::Stab: {
            const float travelDist = glm::length(position - m_lockedPosition);
            if (travelDist > m_profile.stabTravelDistance * armScale) {
                m_attackActive = true;
            }
            break;
        }
        case AttackType::Slash: {
            const glm::fvec3 zNow = rotation * glm::fvec3(0.0f, 0.0f, 1.0f);
            const float dotProduct = glm::clamp(glm::dot(zNow, m_lockedAngle), -1.0f, 1.0f);
            const float angDifference = std::acos(dotProduct);
            if (angDifference > m_profile.slashTravelAngle) {
                m_attackActive = true;
            }
            break;
        }
        default:
            break;
    }
}

inline void WeaponMotionAnalyser::updateSwingPower() {
    if (!m_attackActive) {
        m_swingPower = 0.0f;
        return;
    }

    // Track peak velocity during active attack
    float currentVelocity = 0.0f;
    float referenceVelocity = 1.0f;

    switch (m_lockedAttackType) {
        case AttackType::Slash:
            currentVelocity = glm::length(glm::fvec3(m_localAngVel.x, m_localAngVel.y, 0.0f));
            referenceVelocity = REFERENCE_SLASH_VELOCITY;
            break;
        case AttackType::Stab: {
            currentVelocity = std::abs(m_localLinVel.z);
            referenceVelocity = REFERENCE_STAB_VELOCITY;
            m_peakAttackVelocity = std::max(m_peakAttackVelocity, currentVelocity);

            float straightness = 0.0f;
            if (glm::length2(m_localLinVel) >= 1e-8f) {
                straightness = std::abs(glm::normalize(m_localLinVel).z);
            }

            const float normalizedStraightness = glm::clamp((straightness - m_profile.stabLinearSteadinessThreshold) / (1.0f - m_profile.stabLinearSteadinessThreshold), 0.0f, 1.0f);
            const float straightnessFactor = std::lerp(0.70f, 1.0f, normalizedStraightness);
            m_swingPower = std::clamp(m_peakAttackVelocity / referenceVelocity, 0.0f, 1.0f) * straightnessFactor;
            break;
        }
        default:
            break;
    }

    if (m_lockedAttackType != AttackType::Stab) {
        m_peakAttackVelocity = std::max(m_peakAttackVelocity, currentVelocity);
        m_swingPower = std::clamp(m_peakAttackVelocity / referenceVelocity, 0.0f, 1.0f);
    }
}

inline void WeaponMotionAnalyser::Update(
    const XrSpaceLocation& handLocation,
    const XrSpaceVelocity& handVelocity,
    const glm::fmat4& headsetMtx,
    XrTime inputTime)
{
    const glm::fvec3 linearVelocity = ToGLM(handVelocity.linearVelocity);
    const glm::fvec3 angularVelocity = ToGLM(handVelocity.angularVelocity);
    const glm::fquat rotation = ToGLM(handLocation.pose.orientation);
    const glm::fvec3 position = ToGLM(handLocation.pose.position);
    const glm::fvec3 headsetPosition = glm::fvec3(headsetMtx[3]);

    const float dt = (m_prevSampleTime > 0)
        ? static_cast<float>(inputTime - m_prevSampleTime) / 1e9f
        : 0.0f;

    // ---- Hold on tracking dropouts ----
    // Untracked samples carry garbage positions that would poison calibration and derivatives.
    constexpr XrSpaceLocationFlags requiredFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    if ((handLocation.locationFlags & requiredFlags) != requiredFlags) {
        computeLocalSignals(linearVelocity, angularVelocity, rotation, dt);
        m_prevAngularVelocity = angularVelocity;
        m_prevLocalLinVel = m_localLinVel;
        m_prevSampleTime = inputTime;
        return;
    }

    // ---- Compute all signals ----
    computeLocalSignals(linearVelocity, angularVelocity, rotation, dt);
    m_angularDrift = computeAngularDrift(angularVelocity, dt);
    computeSmoothedSignals(dt);

    if (dt > 0.0f) {
        m_smoothedFrameDt = (m_smoothedFrameDt == 0.0f) ? dt : std::lerp(m_smoothedFrameDt, dt, 1.0f - std::exp(-dt / 0.5f));
    }

    // ---- Auto-calibrate arm length ----
    const float handToHeadDist = glm::distance(position, headsetPosition);
    if (handToHeadDist <= 1.2f) {
        updateArmCalibration(handToHeadDist);
    }

    // ---- Store sample ----
    m_samples[m_sampleIdx] = {
        .time = inputTime,
        .position = position,
        .rotation = rotation,
        .localLinearVelocity = m_localLinVel,
        .localAngularVelocity = m_localAngVel,
        .localLinearAcceleration = m_localLinAccel,
        .smoothedLinearAcceleration = m_smoothedLinAccel,
        .smoothedFlatAngAcc = m_smoothedFlatAngAcc,
        .smoothedAngularDrift = m_smoothedAngDrift,
        .attackType = AttackType::None,
        .handSpeed = m_handSpeed,
    };
    m_lastSampleIdx = m_sampleIdx;
    m_sampleIdx = (m_sampleIdx + 1) % MAX_SAMPLES;

    // ---- Run detection pipeline ----
    // Clamped so a menu pause or frame spike cannot dump seconds into the accumulators.
    const float dtAccum = std::min(dt, 0.05f);
    detectAttackType(position, rotation, inputTime, dtAccum);
    checkAttackSteadiness(inputTime, dtAccum);
    updateAttackActivity(rotation, position);
    updateSwingPower();

    // Write attack state into the sample for debug overlay
    m_samples[m_lastSampleIdx].attackType = IsAttacking() ? m_lockedAttackType : AttackType::None;

    // ---- Save state for next frame ----
    m_prevAngularVelocity = angularVelocity;
    m_prevLocalLinVel = m_localLinVel;
    m_prevSampleTime = inputTime;
}

inline void WeaponMotionAnalyser::ResetSwing() {
    m_goodSwingAccumTime = 0.0f;
    m_lastGoodSwingTime = 0;
    if (m_lockedAttackType == AttackType::Slash) {
        m_lockedAttackType = AttackType::None;
    }
}

inline void WeaponMotionAnalyser::ResetStab() {
    m_goodStabAccumTime = 0.0f;
    m_lastGoodStabTime = 0;
    if (m_lockedAttackType == AttackType::Stab) {
        m_lockedAttackType = AttackType::None;
    }
}

inline void WeaponMotionAnalyser::Reset() {
    m_samples.fill({});
    m_sampleIdx = 0;
    m_lastSampleIdx = 0;
    m_badSampleAccumTime = 0.0f;
    m_attackActive = false;
    m_swingPower = 0.0f;
    m_peakAttackVelocity = 0.0f;
    m_smoothedLinAccel = {};
    m_smoothedLinAccelSlow = {};
    m_smoothedFlatAngAcc = 0.0f;
    m_smoothedAngDrift = 0.0f;
    m_lockTime = 0;
    m_prevLocalLinVel = {};
    m_prevFlatAngVel = {};
    m_prevAngularVelocity = {};
    m_prevSampleTime = 0;
    ResetSwing();
    ResetStab();
}

inline void WeaponMotionAnalyser::ResetIfWeaponTypeChanged(WeaponType weaponType) {
    if (m_weaponType != weaponType) {
        m_weaponType = weaponType;
        Reset();
    }
}

inline WeaponDebugSnapshot WeaponMotionAnalyser::GetDebugSnapshot() const {
    WeaponDebugSnapshot snapshot = {};
    snapshot.weaponType = m_weaponType;
    snapshot.lockedAttackType = m_lockedAttackType;
    snapshot.sampleTime = m_samples[m_lastSampleIdx].time;
    snapshot.attackActive = m_attackActive;
    snapshot.hitboxEnabled = m_isHitboxEnabled;
    snapshot.swingPower = m_swingPower;
    snapshot.badSampleAccumTime = m_badSampleAccumTime;
    snapshot.maxBadDuration = std::max(m_profile.maxBadDuration, 2.5f * m_smoothedFrameDt);
    snapshot.calibratedArmLength = m_calibratedArmLength;

    snapshot.forwardStabSpeed = -m_localLinVel.z;
    snapshot.stabSpeedThreshold = m_profile.stabSpeedThreshold;
    snapshot.forwardStabAcceleration = -m_smoothedLinAccelSlow.z;
    snapshot.stabAccThreshold = m_profile.stabAccThreshold;
    snapshot.stabAngularXY = glm::length(glm::fvec3(m_localAngVel.x, m_localAngVel.y, 0.0f));
    snapshot.stabAngularSteadinessThreshold = m_profile.stabAngularSteadinessThreshold;
    snapshot.goodStabAccumTime = m_goodStabAccumTime;
    snapshot.minGoodStabDuration = m_profile.minGoodStabDuration;

    if (glm::length2(m_localLinVel) >= 1e-8f) {
        snapshot.stabDirectionAbsZ = std::abs(glm::normalize(m_localLinVel).z);
    }
    snapshot.stabLinearSteadinessThreshold = m_profile.stabLinearSteadinessThreshold;

    snapshot.slashAngularSpeed = glm::length(glm::fvec3(m_localAngVel.x, m_localAngVel.y, 0.0f));
    snapshot.slashSpeedThreshold = m_profile.slashSpeedThreshold;
    snapshot.slashAngularAcceleration = m_smoothedFlatAngAcc;
    snapshot.slashAccThreshold = m_profile.slashAccThreshold;
    snapshot.slashVelocityThreshold = m_profile.slashVelocityThreshold;
    snapshot.slashAngularDrift = m_smoothedAngDrift;
    snapshot.slashAccDriftThreshold = m_profile.slashAccDriftThreshold;
    snapshot.goodSwingAccumTime = m_goodSwingAccumTime;
    snapshot.minGoodSwingDuration = m_profile.minGoodSwingDuration;

    snapshot.stabSpeedOk = snapshot.forwardStabSpeed >= snapshot.stabSpeedThreshold;
    snapshot.stabAccelOk = snapshot.forwardStabAcceleration >= snapshot.stabAccThreshold;
    snapshot.stabDirectionOk = snapshot.stabDirectionAbsZ >= snapshot.stabLinearSteadinessThreshold;
    snapshot.stabAngularOk = snapshot.stabAngularXY <= snapshot.stabAngularSteadinessThreshold;
    snapshot.stabDurationOk = snapshot.goodStabAccumTime >= snapshot.minGoodStabDuration;
    snapshot.slashSpeedOk = snapshot.slashAngularSpeed >= snapshot.slashSpeedThreshold;
    snapshot.slashAccelOk = snapshot.slashAngularAcceleration >= snapshot.slashAccThreshold;
    snapshot.slashVelocityOk = snapshot.slashAngularSpeed >= snapshot.slashVelocityThreshold;
    snapshot.slashDriftOk = snapshot.slashAngularDrift <= snapshot.slashAccDriftThreshold;
    snapshot.slashDurationOk = snapshot.goodSwingAccumTime >= snapshot.minGoodSwingDuration;
    return snapshot;
}
