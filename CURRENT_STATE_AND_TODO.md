# Current State and To-Do List

Date: 2026-06-21
Project: `lidar_rostami`

This file is a short, practical status note for continuing work without rereading the full handoff file.

---

## Current State

### 1. AI / Combat / Cover

The main AI systems are mostly functional now:

- Enemies can move to the quest zone.
- Allies and leader can detect enemies.
- Damage and enemy death are working.
- Cover system can find real cover after several fixes.
- World Partition conversion reduced memory usage significantly.
- Quest system runs again after making quest-critical actors always loaded / streamed correctly.

Remaining AI/cover concerns:

- Cover selection still needs polish in some cases.
- Leader behavior should be regression-tested after animation work.
- Engage state may still shoot into landscape/sandbags if the selected cover blocks line of sight.
- A future fix should make Engage detect repeated blocked shots and request a better cover/peek position.

---

### 2. World Partition / Landscape

World Partition was introduced to reduce landscape VRAM/memory usage.

Important notes:

- Landscape memory was the biggest VRAM/system memory issue, not the 102k-poly leader.
- Terrain heightmaps/weightmaps were very large.
- World Partition helped a lot.
- Quest actors and navigation need special handling.

Important World Partition setup reminders:

- QuestDirector / QuestMission / critical quest actors should be **non-spatially loaded**.
- Active quest zone should be loaded by a streaming source.
- Player, leader, and/or active quest zone should act as streaming sources.
- NavMesh must exist in loaded cells.
- If using Navigation Invokers, add them to player/leader/enemies/quest streaming source as needed.

Known warning:

```text
Material expects texture T_Soil_05_M to be Virtual
```

This should be fixed later by either making that texture virtual or changing the material sample type.

---

### 3. Character / Mesh / Clothing

The current character is **not MetaHuman**. It is:

```text
Character Creator / CC Auto Setup + Marvelous Designer outfit imported to Unreal
```

Current character notes:

- Character works with CC skeleton.
- Retargeting Rifle Pro to this character now works after retarget fixes.
- Some body/clothing clipping existed because body skin section remained under outfit.
- Recommended fix is to duplicate the skeletal mesh and delete/hide remaining body triangles under clothing except neck/hands/head.
- Cloth Paint is **not** for hiding body mesh; it is for cloth simulation weights.

---

### 4. Rifle Shooter Pro Retargeting

Rifle Shooter Pro 4.27 animations were retargeted to the CC character.

Important fixes already discovered:

- Set Retarget Root to `pelvis` in both IK Rigs.
- Remove/root-pelvis one-bone chains if they cause warnings.
- Do not retarget all finger/metacarpal chains early.
- Arm chains were changed to end at `lowerarm_l/r` instead of `hand_l/r` to avoid wrist twisting.
- Spine mismatch was fixed by chain setup / post-process preview awareness.
- Character now previews retargeted animations correctly.

Stage 1 complete:

- Retargeting proof worked.

Stage 2 complete:

- Base rifle locomotion BlendSpaces / AnimBP states were set up.

Stage 3 complete:

- Fire montage and death montage setup confirmed.
- Fire montage was playing but was subtle.
- Death montage issue was fixed by using FullBody slot and delaying ragdoll.

Stage 4 mostly complete:

- Cover states and cover movement setup were added.
- CoverLow/High hidden/peek states were added.
- Cover movement variables were added in AnimBP.
- Cover aim offsets were started/partially set up.

---

### 5. Weapon / G3 Setup

A G3 rifle mesh is being used for leader/allies.

Current weapon state:

- G3 imported as Static Mesh.
- G3 attached to the character right hand and follows animation.
- Weapon stock clipping was fixed by adjusting attachment/offset.
- G3 has or should have these sockets:

```text
Muzzle
LeftHandIK
Magazine
ShellEject
```

Important concept:

- `hand_l` is the wrist joint, not palm center.
- The weapon `LeftHandIK` socket should represent the **wrist target position**, not where the fingers touch.

---

### 6. Current Blocking Issue: Left Hand IK / Grip

The current active problem is the weapon hand setup.

What works:

- G3 attaches to right hand.
- Hand location can follow the weapon socket.
- Some elbow/wrist tuning has been attempted.

What is problematic:

- Elbow direction can become weird depending on pose.
- Wrist/forearm can twist if hand rotation is forced without forearm distribution.
- Finger grip is not done.
- Previous C++ helper approach caused confusion because BP_Leader values and AnimBP values conflicted.

Current direction:

- User wants to avoid broad C++ helper systems that create conflicts.
- User prefers a simple, stable, preview/AnimBP-oriented workflow.
- A YouTube guide uses a combination of IK bone and FABRIK; the project is exploring that approach.
- The weapon is not visible in AnimBP preview by default; adding the G3 as a preview asset to the `RifleSocket` is needed for visual editing.

Current attempted method:

```text
Get Left Hand IK World Transform
Get hand_r socket/world transform
Convert LeftHandIK transform relative to hand_r
Use FABRIK with Effector Transform Space = Bone Space and Effector Target = hand_r
```

If `Transform To Bone Space` is unavailable, use:

```text
Make Relative Transform
```

or manually:

```text
Inverse Transform Location
Inverse Transform Rotation
Make Transform
```

---

## Important Files Created During This Session

- `AI_SYSTEM_CHANGELOG_AND_HANDOFF.md` — full detailed handoff.
- `ANIMATION_STAGE4_COVER_FIX_NOTES.md` — cover state timing fix notes.
- `ANIMATION_STAGE4C_MONTAGE_VARIANTS.md` — cover fire/reload montage variables.
- `WEAPON_IK_TUNING_GUIDE.md` — weapon IK tuning notes.
- `ANIMATION_BLUEPRINT_AUTOMATION_GUIDE.md` — optional C++ AnimInstance helper notes.
- `ANIMATION_IK_SIMPLIFIED_ANIMBP_ONLY_GUIDE.md` — simplified AnimBP-only IK approach.
- `WEAPON_IK_PREVIEW_WORKFLOW.md` — weapon preview asset/socket workflow.
- `CURRENT_STATE_AND_TODO.md` — this file.

---

## To-Do List

## A. Stabilize Weapon Preview Setup

1. Add G3 as preview asset to character skeleton:

```text
hand_r → RifleSocket → Add Preview Asset → SM_G3
```

2. Verify G3 is visible in animation preview.
3. Verify G3 orientation/scale is correct in preview.
4. Do not use broad C++ IK runtime helper for now unless explicitly needed.

---

## B. Finish Left Hand IK Setup

Goal:

```text
left hand/wrist follows G3 LeftHandIK socket without elbow flips
```

Steps:

1. Use FABRIK guide-style setup if possible:

```text
LeftHandIK socket transform relative to hand_r
FABRIK Root = upperarm_l
FABRIK Tip = hand_l
Effector Space = Bone Space
Effector Target = hand_r
```

2. If `Transform To Bone Space` node is unavailable, use:

```text
Get LeftHandIK World Transform
Get hand_r World Transform
Make Relative Transform
Set LeftHandIKTransform_BS
```

or manual equivalent:

```text
Inverse Transform Location
Inverse Transform Rotation
Make Transform
```

3. Test FABRIK in gameplay and/or preview once G3 preview is visible.
4. If elbow still bends badly, test Two Bone IK again with a stable joint target.
5. Avoid multiple IK systems active at once.

---

## C. Finger / Grip Pose

After wrist/hand location is stable:

1. Create a one-frame/additive rifle grip pose:

```text
A_RifleGrip_Hands
```

2. Apply only to finger/hand bones using Layered Blend Per Bone.
3. Do not retarget all finger chains from source animations.
4. Keep fingers separate from IK location solving.

---

## D. Visual Aiming / Rifle Points at Target

After hand IK is acceptable:

1. Add/verify standing/crouch aim offsets.
2. Ensure rifle visually points toward target while AI aims.
3. Avoid using muzzle socket as the gameplay trace source if animation pose is not ready.
4. Continue using logical AI muzzle for gameplay if needed, but use weapon socket for muzzle VFX.

---

## E. Cover Combat Polish

1. Verify CoverLow/High hidden/peek states still work after weapon IK changes.
2. Ensure cover movement no longer slides.
3. Add blocked-shot detection to `STTask_EngageTarget`:

```text
if repeated shots hit landscape/sandbag/no health target for N seconds:
    stop firing
    request new cover / reposition
```

4. Replace temporary `FireMontage_CoverLowOver = M_Rifle_Fire_Crouch` when a real over-cover fire montage/additive is ready.

---

## F. Animation Cleanup

1. Keep using in-place animations for AI locomotion.
2. Do not use marketplace demo Blueprints.
3. Use only retargeted animations in the real AnimBP.
4. Keep root motion for later/special moves only.
5. Avoid compiling AnimBP during PIE; it can crash.

---

## G. World Partition / Nav Follow-Up

1. Confirm all active mission actors are loaded/non-spatial as needed.
2. Confirm navmesh exists in streamed cells.
3. Add streaming source and/or navigation invoker to active quest zone if needed.
4. Fix material warning:

```text
Material expects T_Soil_05_M to be Virtual
```

5. Re-run memory report after final world partition setup.

---

## H. C++ Cleanup Later

There are now some optional helper systems that may not be used immediately:

```text
USoldierAnimInstance
GetSoldierAnimRuntimeData
GetWeaponIKRuntimeData
```

Do not remove them yet; they are harmless if unused. Later decide whether to:

- use them properly,
- or remove them to reduce complexity.

---

## Current Recommendation

Do **not** make broad code changes now.

Next practical step:

```text
Finish G3 preview asset setup and use the YouTube guide workflow to solve hand placement with the weapon visible.
```

Then move to finger grip pose.
