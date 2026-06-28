# Weapon IK Tuning Guide

Date: 2026-06-19

## Purpose

This project uses Character Creator / Marvelous characters and separate weapon meshes. Weapon alignment should be tuned through easy Blueprint-exposed variables instead of rebuilding AnimBP math every time.

The C++ side now exposes centralized weapon attachment and left-hand IK data on `ASoldierCharacter`.

---

## Files Changed

```text
Source/SquadAI/Public/Characters/SoldierCharacter.h
Source/SquadAI/Private/Characters/SoldierCharacter.cpp
```

---

## Weapon Visual Setup Variables

These are available on BP_Leader / BP_Enemy / BP_Ally under Soldier Weapon categories.

### EquippedWeaponMesh

```cpp
UMeshComponent* EquippedWeaponMesh
```

Assign this to the weapon mesh component, e.g. `G3Mesh`.

### Socket Names

```cpp
WeaponAttachSocketName = "RifleSocket"
WeaponLeftHandIKSocketName = "LeftHandIK"
WeaponMuzzleSocketName = "Muzzle"
```

Required sockets:

- Character skeleton/socket: `RifleSocket` on `hand_r`.
- Weapon mesh sockets: `LeftHandIK`, `Muzzle`.
- Optional weapon mesh sockets: `Magazine`, `ShellEject`.

---

## Automatic Weapon Attachment Tunables

If `bAutoAttachEquippedWeaponToMesh` is true, calling `SetEquippedWeaponMesh(G3Mesh)` will attach the weapon to the character mesh at `WeaponAttachSocketName` and apply offsets.

```cpp
bAutoAttachEquippedWeaponToMesh = true
WeaponAttachLocationOffset = (0,0,0)
WeaponAttachRotationOffset = (0,0,0)
WeaponAttachScale = (1,1,1)
```

Use these to fix stock clipping, grip position, and rifle angle without editing the skeleton socket.

Recommended workflow:

1. Attach G3Mesh to leader with `SetEquippedWeaponMesh(G3Mesh)`.
2. Tune `WeaponAttachLocationOffset` and `WeaponAttachRotationOffset` in BP_Leader.
3. Do not scale the rifle unless the import scale is actually wrong.

---

## Left Hand IK Tunables

### Basic enable/alpha

```cpp
bEnableLeftHandWeaponIK = true
LeftHandWeaponIKAlpha = 1.0
```

### Effector offset

```cpp
LeftHandIKEffectorLocationOffset_CS
```

Component-space offset added after reading the weapon `LeftHandIK` socket. Use for small hand placement corrections without moving the weapon socket.

### Elbow target

```cpp
LeftElbowJointTarget_BoneSpace = (0, -80, 0)
```

Use this with AnimBP Two Bone IK:

```text
Joint Target Location Space = Bone Space
Joint Target Space Bone Name = upperarm_l
Joint Target Location = LeftElbowJointTargetBS
```

Common values to test:

```text
(0, -80, 0)
(0, 80, 0)
(80, 0, 0)
(-80, 0, 0)
```

Use larger values like 200 temporarily to see direction changes, then reduce.

### Optional hand rotation driving

```cpp
bUseLeftHandIKRotation = false
LeftHandIKRotationAlpha = 1.0
LeftHandIKRotationOffset = (0,0,0)
```

Keep `bUseLeftHandIKRotation = false` until location and elbow are correct. If enabled, AnimBP can use `LeftHandEffectorRotationCS` with `Transform Modify Bone` on `hand_l`.

---

## AnimBP Runtime Data Helper

Use this function in AnimBP Event Graph:

```cpp
GetWeaponIKRuntimeData()
```

It returns:

```cpp
bEnabled
Alpha
LeftHandEffectorLocationCS
LeftHandEffectorRotationCS
LeftElbowJointTargetBS
bUseLeftHandRotation
LeftHandRotationAlpha
```

This removes the need for manual world-to-component transform conversion inside AnimBP.

---

## Recommended AnimBP Variables

Create these in `ABP_CC_Soldier_Basic`:

```text
bEnableLeftHandIK : bool
LeftHandIKAlpha : float
LeftHandIKLocation_CS : vector
LeftHandIKRotation_CS : rotator
LeftElbowJointTarget_BS : vector
bUseLeftHandRotation : bool
LeftHandRotationAlpha : float
```

Event Graph:

```text
As SoldierCharacter
→ GetWeaponIKRuntimeData
→ Break WeaponIKRuntimeData
→ Set variables above
```

---

## Recommended AnimGraph Order

The IK should be after locomotion, slots, montages, and cover blending.

```text
Final Animation Pose
→ Convert Local To Component Space
→ Two Bone IK
→ optional Transform Modify Bone for hand_l rotation
→ Convert Component To Local Space
→ Output Pose
```

Two Bone IK settings:

```text
IK Bone = hand_l
Effector Location = LeftHandIKLocation_CS
Effector Location Space = Component Space
Joint Target Location = LeftElbowJointTarget_BS
Joint Target Location Space = Bone Space
Joint Target Space Bone Name = upperarm_l
Alpha = LeftHandIKAlpha
Allow Stretching = false
Take Rotation from Effector Space = false initially
Maintain Effector Rel Rot = true initially
```

Optional hand rotation node after Two Bone IK:

```text
Transform Modify Bone
Bone = hand_l
Rotation = LeftHandIKRotation_CS
Rotation Mode = Replace Existing or Add to Existing
Rotation Space = Component Space
Alpha = LeftHandRotationAlpha if bUseLeftHandRotation
```

Only enable this after elbow/location are stable.

---

## Tuning Order

1. Tune `WeaponAttachLocationOffset` / `WeaponAttachRotationOffset` until rifle sits correctly in right hand and stock does not clip badly.
2. Move weapon `LeftHandIK` socket to the **wrist target** position, not palm/finger surface.
3. Tune `LeftElbowJointTarget_BoneSpace` until elbow bends naturally.
4. Tune `LeftHandIKEffectorLocationOffset_CS` for small left hand placement corrections.
5. Only then enable `bUseLeftHandIKRotation` and tune `LeftHandIKRotationOffset` / weapon socket rotation.
6. Add a finger/grip pose later. IK does not curl fingers.

---

## Important Concept

`hand_l` is usually the wrist joint. Therefore the weapon `LeftHandIK` socket should be placed where the **wrist** should be, not where the palm/fingers touch the foregrip.

Finger gripping is a separate stage using additive pose or Control Rig.
