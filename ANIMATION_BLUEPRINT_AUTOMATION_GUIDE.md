# Animation Blueprint Automation Guide

Date: 2026-06-19

## Purpose

The goal is to minimize Blueprint math and make soldier animation setup mostly data-driven from C++ helper functions.

Instead of manually calculating speed, direction, cover movement, aim offsets, and weapon IK in the AnimBP, use a single helper:

```cpp
ASoldierCharacter::GetSoldierAnimRuntimeData(...)
```

---

## Files Changed

```text
Source/SquadAI/Public/Characters/SoldierCharacter.h
Source/SquadAI/Private/Characters/SoldierCharacter.cpp
```

---

## New Runtime Data Struct

Added:

```cpp
FSoldierAnimRuntimeData
```

It contains:

```cpp
float Speed;
float Direction;
bool bIsAlive;
bool bIsCrouched;
bool bCombatReady;
bool bWantsToFire;

bool bIsInCover;
bool bIsCrouchingInCover;
bool bIsPeekingFromCover;
bool bCoverIsLow;
bool bCoverSideIsLeft;
bool bCoverPeekIsOver;
bool bIsMovingInCover;
bool bIsAdjustingCover;
float CoverMoveDirection;

float AimYaw;
float AimPitch;

FWeaponIKRuntimeData WeaponIK;
```

---

## New AnimBP Helper Function

Added:

```cpp
FSoldierAnimRuntimeData GetSoldierAnimRuntimeData(
    float CoverMoveSpeedThreshold = 20.f,
    float CoverAdjustDirectionThreshold = 0.35f,
    float CombatThreatConfidence = 0.25f
) const;
```

This function computes:

- locomotion speed,
- locomotion direction,
- alive/crouch/combat state,
- cover booleans,
- cover side/peek type,
- moving-in-cover state,
- cover move direction,
- cover adjustment state,
- aim yaw/pitch from `AimComp->GetSpineAimOffset()`,
- weapon IK runtime data.

---

## Recommended AnimBP Event Graph Setup

In `ABP_CC_Soldier_Basic`:

1. Keep `Try Get Pawn Owner`.
2. Cast to `SoldierCharacter`.
3. From `As SoldierCharacter`, call:

```text
Get Soldier Anim Runtime Data
```

4. Break the returned struct.
5. Set your AnimBP variables from it.

Recommended AnimBP variables:

```text
Speed : float
Direction : float
bIsAlive : bool
bIsCrouched : bool
bCombatReady : bool
bWantsToFire : bool

bIsInCover : bool
bIsCrouchingInCover : bool
bIsPeekingFromCover : bool
bCoverIsLow : bool
bCoverSideIsLeft : bool
bCoverPeekIsOver : bool
bIsMovingInCover : bool
bIsAdjustingCover : bool
CoverMoveDirection : float

AimYaw : float
AimPitch : float

bEnableLeftHandIK : bool
LeftHandIKAlpha : float
LeftHandIKLocation_CS : vector
LeftHandIKRotation_CS : rotator
LeftElbowJointTarget_BS : vector
bUseLeftHandRotation : bool
LeftHandRotationAlpha : float
```

From `WeaponIK` inside the struct:

```text
WeaponIK.bEnabled                 → bEnableLeftHandIK
WeaponIK.Alpha                    → LeftHandIKAlpha
WeaponIK.LeftHandEffectorLocationCS → LeftHandIKLocation_CS
WeaponIK.LeftHandEffectorRotationCS → LeftHandIKRotation_CS
WeaponIK.LeftElbowJointTargetBS     → LeftElbowJointTarget_BS
WeaponIK.bUseLeftHandRotation       → bUseLeftHandRotation
WeaponIK.LeftHandRotationAlpha      → LeftHandRotationAlpha
```

This replaces previous Blueprint calculations such as:

- Get Velocity → Vector Length XY,
- Calculate Direction,
- manual CoverMoveDirection dot product,
- manual AimComp GetSpineAimOffset,
- manual weapon socket transform conversion.

---

## Why This Helps

Before this helper, the AnimBP needed many nodes:

```text
Velocity math
Direction math
Cover booleans
Cover enum comparisons
Cover move direction dot product
Aim offset extraction
Weapon IK transform conversion
```

Now all of that is centralized in C++ and exposed as one Blueprint node.

---

## Tunable Inputs

When calling `GetSoldierAnimRuntimeData`, the AnimBP can pass:

```text
CoverMoveSpeedThreshold = 20
CoverAdjustDirectionThreshold = 0.35
CombatThreatConfidence = 0.25
```

Recommended defaults:

```text
CoverMoveSpeedThreshold = 20
CoverAdjustDirectionThreshold = 0.35
CombatThreatConfidence = 0.25
```

If cover movement adjustment triggers too often:

```text
CoverAdjustDirectionThreshold = 0.25
```

If it rarely triggers:

```text
CoverAdjustDirectionThreshold = 0.45
```

---

## Weapon IK Tuning Still Lives on Character Blueprint

Tune these on `BP_Leader`, `BP_Enemy`, etc.:

```text
WeaponAttachLocationOffset
WeaponAttachRotationOffset
LeftHandIKEffectorLocationOffset_CS
LeftElbowJointTarget_BoneSpace
bUseLeftHandIKRotation
LeftHandIKRotationOffset
LeftHandWeaponIKAlpha
```

The AnimBP should consume the output data, not do the math.

---

## Recommended AnimGraph Order

```text
Locomotion / Cover / Montage Pose
→ Convert Local To Component Space
→ Two Bone IK for hand_l
→ optional Transform Modify Bone for hand_l rotation
→ Convert Component To Local Space
→ Output Pose
```

Two Bone IK should use:

```text
IK Bone = hand_l
Effector Location = LeftHandIKLocation_CS
Effector Location Space = Component Space
Joint Target Location = LeftElbowJointTarget_BS
Joint Target Location Space = Bone Space
Joint Target Space Bone Name = upperarm_l
Alpha = LeftHandIKAlpha
```

Optional hand rotation node:

```text
Transform Modify Bone
Bone = hand_l
Rotation = LeftHandIKRotation_CS
Rotation Space = Component Space
Alpha = LeftHandRotationAlpha
```

Only enable rotation after the elbow/location solve is good.
