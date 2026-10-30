# Precision - Weapon Trails Only

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Skyrim SE](https://img.shields.io/badge/Skyrim%20SE-1.6.1170-blue.svg)](https://store.steampowered.com/app/489830)
[![Precision Baseline](https://img.shields.io/badge/Precision%20Baseline-v2.0.4%20(e094209)-brightgreen.svg)](https://github.com/ersh1/Precision/commit/e094209bff698a0739b3a881dfdfa3b1697267d8)

A visual-only fork of Ersh's renowned [Precision - Accurate Melee Collisions](https://github.com/ersh1/Precision) (v2.0.4 baseline, commit `e094209`) specifically adapted for **The Elder Scrolls V: Skyrim Special Edition 1.6.1170**.

---

## Overview

**Precision - Weapon Trails Only** isolates and retains Precision's weapon swing visual trails (including vanilla slash trails, elemental enchantment glowing trails for fire, frost, and shock, and custom TOML trail definitions) while completely disabling all gameplay, combat, and physics interventions.

By decoupling the visual trail system from Havok physics and combat collision detection:
- **Vanilla Melee Combat Restored**: Restores 100% vanilla hit detection, damage resolution, weapon ranges, and perks (such as Sweep).
- **Zero Combat Interference**: Strips out attack collision checks, hitstop (hit pause), weapon recoil, camera shake, hit impulse, jump invincibility frames, upper-body pitch adjustments, and AI combat reach overrides.
- **Pure Visual Enhancement**: Retains weapon trail effects with zero Havok performance overhead or script bloat.
- **Streamlined MCM**: Replaces the full combat menu with a clean, single-page MCM dedicated exclusively to weapon trail tuning.

---

## Technical Highlights & Architecture

### 1. Hook Whitelist Isolation
Only 3 visual-critical hooks are installed:
- `UpdateHooks::Hook()`: Retains the main loop frame update (`_Nullsub`), while stripping out hitstop animation modifications and upper-body pitch overrides.
- `HavokHooks::Hook()`: Retains only the `_ProcessHavokHitJobs` scheduling call to flag visual actor updates. The callback invokes the original game function untouched and never executes Havok contact listeners, collision filtering, or skeleton cloning.
- `FirstPersonStateHook::Hook()`: Retains visual anchor cleanup when switching between first-person and third-person perspectives.
- **Omitted Hooks**: `AttackHooks`, `AIHooks`, `CameraShakeHook`, `MovementHook`, and all ragdoll/Havok collision hooks are completely absent.
- **Runtime Validation**: Validates Skyrim runtime `1.6.1170.0` and verifies opcode `0xE8` (call) at relocation sites before patching.

### 2. Pure Visual `ActiveActor` & Event Queue
- `ActiveActor` provides a visual-only constructor that tracks animation graph states, current cell, and scale generation counter without cloning Havok physics skeletons or allocating collision groups.
- Animation events (`weaponSwing`, `weaponLeftSwing`, `Collision_*`, `attackStop`, `Unequip`, `weaponSheathe`) are captured thread-safely in `visualEvents` and dispatched on the main thread during `PrecisionHandler::Update`.
- Attack data queries are made read-only, preventing mutations to active actor combat states.

### 3. Pure Visual `AttackCollision` & Geometry Fallback
- `AttackCollision::Add` creates `NiNode` visual anchors exclusively for `WEAPON` and `SHIELD` nodes (ignoring unarmed attacks and shield bashes).
- Provides purely numerical weapon dimension and tip offset fallbacks directly to `AttackTrail`, completely skipping `bhkCollisionObject` and `hkpRigidBody` creation.
- Natural fade-out on detachment via existing trail lifetime decay.

### 4. Settings Lock-Off & ModAPI Stubs
- `Settings::ReadSettings()` strictly locks non-visual settings (`bAttackCollisionsEnabled`, `bEnableHitstop`, `bRecoilPlayer`, `bRecoilNPC`, `bApplyImpulseOnHit`, `bHookAIWeaponReach`, `bDisableCharacterBumper`, `bEnableJumpIframes`, `bDebug`) to `false`.
- External API calls (`ApplyHitImpulse`, `AddHitstop`, `ApplyCameraShake`) are stubbed out to safely no-op, preserving binary compatibility without side effects.

---

## Requirements

### Runtime Requirements
- [The Elder Scrolls V: Skyrim Special Edition](https://store.steampowered.com/app/489830) (v1.6.1170.0)
- [SKSE64](https://skse.silverlock.org/) (matching runtime 1.6.1170)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

### Build Requirements
- [Visual Studio Community 2022](https://visualstudio.microsoft.com/) (Desktop development with C++, MSVC v143+)
- [CMake](https://cmake.org/) (v3.23+)
- [Ninja](https://ninja-build.org/) (optional, recommended)
- [Vcpkg](https://github.com/microsoft/vcpkg) (set `VCPKG_ROOT` environment variable)
- [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG) (set `CommonLibSSEPath_NG` environment variable)

---

## Building from Source

```bash
git clone --recurse-submodules https://github.com/MichaelWang44/Precision-Weapon-Trails-Only.git
cd Precision-Weapon-Trails-Only

# Configure with CMake (Visual Studio 2022 or Ninja)
cmake --preset vs2022-windows

# Build Release
cmake --build build --config Release
```

---

## Credits & Attributions

- **[Ersh](https://github.com/ersh1)**: Original creator of [Precision - Accurate Melee Collisions](https://github.com/ersh1/Precision).
- **[adamhynek](https://github.com/adamhynek)**: Havok integration concepts from [PLANCK](https://github.com/adamhynek/activeragdoll) and [HIGGS](https://github.com/adamhynek/higgs).
- **[mwilsnd](https://github.com/mwilsnd)**: Trajectory rendering inspiration from [SmoothCam](https://github.com/mwilsnd/SkyrimSE-SmoothCam).
- **[CharmedBaryon](https://github.com/CharmedBaryon)**: [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG).

---

## License

This project is licensed under the [MIT License](LICENSE), consistent with the original Precision repository.
Copyright (c) 2023 Ersh.
