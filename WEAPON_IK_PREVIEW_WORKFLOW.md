# Weapon IK Preview Workflow — Socket Preview Asset Approach

Date: 2026-06-21

## Why this exists

The project temporarily experimented with C++-driven weapon IK runtime data. That approach became confusing because BP_Leader tunables and AnimBP variables could overwrite each other.

Current safer workflow:

- Do **not** use `GetWeaponIKRuntimeData()` in AnimBP unless explicitly needed.
- Use the character skeleton's socket preview asset to show the gun in animation/AnimBP preview.
- Fix hand/arm pose visually in the animation/retarget/AnimBP preview with the weapon visible.
- Keep weapon IK tuning mostly in the AnimBP / sockets until stable.

## Code state

The helper functions still exist for future use, but default runtime IK is disabled:

```cpp
bEnableLeftHandWeaponIK = false;
bEnableWeaponGripPose = false;
bAutoAttachEquippedWeaponToMesh = false;
```

This means existing C++ helpers should not drive the leader's hand IK unless you explicitly enable them and wire the AnimBP to consume them.

## How to show the G3 in animation preview

1. Open the character skeleton / skeletal mesh used by your CC character.
2. In the Skeleton Tree, find `hand_r`.
3. Create or select the socket:

```text
RifleSocket
```

4. Right-click `RifleSocket`.
5. Choose:

```text
Add Preview Asset
```

6. Select the G3 static mesh, e.g.:

```text
SM_G3
```

7. Adjust the `RifleSocket` transform until the G3 sits correctly in the right hand.
8. Save the skeleton/mesh.

This preview asset is editor-only and does not change gameplay attachment. It simply makes the gun visible while previewing animations and retargeted poses.

## Alternative preview method

If socket preview asset is unavailable, open the animation editor / AnimBP preview and use:

```text
Preview Scene Settings → Additional Meshes / Attached Assets
```

Attach `SM_G3` to `RifleSocket` if the UI allows it.

## Important

`hand_l` is usually the wrist joint. If you add a `LeftHandIK` socket on the weapon, place it where the **wrist** should be, not where the fingers touch.

Finger curl / grip should be solved later with:

- additive grip pose,
- Control Rig,
- or hand/finger Transform Modify Bone nodes.

## What not to use for now

Avoid using these in the AnimBP until the preview/socket workflow is stable:

```text
GetWeaponIKRuntimeData
Break WeaponIKRuntimeData
C++ dynamic elbow target
C++ additive rotation distribution
```

They are still in code for future use, but disabled by default.
