# Simplified AnimBP-Only Weapon IK Guide

Date: 2026-06-19

## Reason

The project accumulated multiple weapon IK tuning paths:

- BP_Leader tunables
- C++ `GetWeaponIKRuntimeData()`
- AnimBP variables
- Two Bone IK settings
- Transform Modify Bone offsets

This can become confusing and can create apparent conflicts when values are edited in PIE or when AnimBP variables are overwritten every frame from C++.

The user decided all characters will use the same size/rig, so the simplest workflow is:

> Tune weapon IK in the main AnimBP only, once, and reuse that AnimBP for all same-rig characters.

## Important

Do **not** remove the C++ helpers. They can remain unused. The simplified setup just stops using `GetWeaponIKRuntimeData()` in the AnimBP.

## Keep from C++

Still useful:

```text
SetEquippedWeaponMesh(G3Mesh)
GetLeftHandIKMeshSpaceTransform
```

Use `GetLeftHandIKMeshSpaceTransform` only for the weapon socket location/rotation.

## Stop Using in AnimBP

In the AnimBP Event Graph, remove or disconnect:

```text
GetWeaponIKRuntimeData
Break WeaponIKRuntimeData
```

Also remove/disconnect all `Set` nodes that get values from that struct, such as:

```text
Set LeftElbowIKLocation_CS from WeaponIK.LeftElbowJointTargetCS
Set LeftElbowJointTarget_BS from WeaponIK.LeftElbowJointTargetBS
Set LeftForearmAdditiveRotation from WeaponIK.LeftForearmAdditiveRotation
Set LeftHandAdditiveRotation from WeaponIK.LeftHandAdditiveRotation
Set bUseDynamicElbowTarget from WeaponIK.bUseDynamicElbowTarget
```

## Keep in AnimBP Event Graph

Use this minimal weapon socket section:

```text
As SoldierCharacter
→ Get Left Hand IK Mesh Space Transform
→ Break Transform
→ Set bEnableLeftHandIK from Return Value
→ Set LeftHandIKLocation_CS from Location
→ Set LeftHandIKRotation_CS from Rotation, optional
```

That is the only weapon-related value that should come from the character/weapon at runtime.

## Tune in AnimBP Defaults Only

Create/set these variables directly in the AnimBP:

```text
LeftElbowJointTarget_BS : Vector
LeftForearmRotationOffset : Rotator
LeftForearmRotationAlpha : Float
LeftHandRotationOffset : Rotator
LeftHandRotationAlpha : Float
bUseHandRotationCorrection : Bool
```

These are not overwritten by C++; they are edited once in `ABP_CC_Soldier_Basic` and reused for every same-rig/same-size character.

## Recommended AnimGraph

```text
Final Animation Pose
→ Convert Local To Component Space
→ Two Bone IK
→ Transform Modify Bone lowerarm_twist_01_l, optional
→ Transform Modify Bone hand_l, optional
→ Convert Component To Local Space
→ Output Pose
```

## Two Bone IK Settings

```text
IK Bone = hand_l
Effector Location = LeftHandIKLocation_CS
Effector Location Space = Component Space
Joint Target Location = LeftElbowJointTarget_BS
Joint Target Location Space = Bone Space
Joint Target Space Bone Name = upperarm_l
Alpha = bEnableLeftHandIK ? 1.0 : 0.0
Allow Stretching = false
Take Rotation from Effector Space = false
Maintain Effector Rel Rot = true
```

## Suggested Starting Elbow Values

Try one of these in AnimBP defaults:

```text
LeftElbowJointTarget_BS = (0, -80, 0)
LeftElbowJointTarget_BS = (0, 80, 0)
LeftElbowJointTarget_BS = (80, 0, 0)
LeftElbowJointTarget_BS = (-80, 0, 0)
```

Use larger values like 200 only for testing direction.

## Rotation Correction

If elbow is good but wrist/forearm twist is bad, use additive rotation distribution:

1. Transform Modify Bone on `lowerarm_twist_01_l`
2. Transform Modify Bone on `hand_l`

Both should use:

```text
Rotation Mode = Add to Existing
Rotation Space = Bone Space
Translation Mode = Ignore
Scale Mode = Ignore
```

Example values from previous working setup:

```text
LeftForearmRotationOffset = (-70, 0, 0)
LeftForearmRotationAlpha = 0.5 to 1.0

LeftHandRotationOffset = (-30, 10, 9)
LeftHandRotationAlpha = 1.0
```

If wrist twists, reduce forearm or hand alpha.

## Why This Is Simpler

- BP_Leader no longer controls IK tuning.
- AnimBP variables are not overwritten by C++ runtime data.
- One AnimBP setup works for all same-rig/same-size characters.
- Only the weapon socket transform is read at runtime.

## For Different Weapons Later

If G3 and AK need different offsets, use one of these:

1. Duplicate AnimBP per weapon type, simplest.
2. Add a small `WeaponType` enum later.
3. Use a DataAsset per weapon later.

For now, one AnimBP is acceptable because the user is prioritizing speed and same-rig characters.
