# SquadAI / Lidar Rostami — AI, Cover, Combat, Open World, and Animation Handoff

**Last Updated:** 2026-06-17  
**Repository:** `lidar_rostami`  
**Purpose:** Handoff notes for the next developer. This document summarizes the debugging session, code changes, editor-side setup, animation pipeline, World Partition work, known issues, and upcoming stages.

---

## 0. Current High-Level State

The project contains a custom Unreal Engine C++ AI/quest/combat/cover system. During this work session, the following major systems were debugged and improved:

- Enemy mission movement to quest zone.
- Team IDs and hostility/perception logic.
- Direct combat fallback and threat memory.
- Cover system scanning and selection.
- Weapon hit detection, damage, and death.
- Leader path interruption and combat fallback.
- Character Creator / Marvelous Designer character animation setup.
- Rifle Shooter Pro retargeting.
- Stage 2 locomotion AnimBP setup.
- Stage 3 fire/death montage setup.
- World Partition conversion and landscape memory investigation.

The current character pipeline is **Character Creator + Marvelous Designer imported into Unreal via CC Auto Setup**, not MetaHuman.

---

## 1. Original Problems Reported

1. **Enemy did not move** although StateTree showed `Enemy Move To Attack Point`.
2. **Enemy/allies/leader ignored each other** even when placed close together.
3. **StateTree transitions were unreliable**.
4. **Cover system reported `no real cover`** around sandbags/landscape ridges.
5. **Weapon traces showed red lines but enemies did not die**.
6. **Dead enemies could keep shooting briefly**.
7. **Leader rushed into enemies / stayed in path-follow**.
8. **Animation retargeting from Rifle Shooter Pro to CC character had spine and wrist issues**.
9. **VRAM usage became high after importing/previewing animations and CC character**.
10. **World Partition conversion reduced memory but initially broke nav/quest loading**.

---

## 2. Major Files Modified

### AI Controller / Threat / Combat

- `Source/SquadAI/Public/AI/SoldierAIController.h`
- `Source/SquadAI/Private/AI/SoldierAIController.cpp`

### StateTree Tasks

- `Source/SquadAI/Public/AI/StateTreeTasks/STTask_EnemyMission.h`
- `Source/SquadAI/Private/AI/StateTreeTasks/STTask_EnemyMission.cpp`
- `Source/SquadAI/Public/AI/StateTreeTasks/STTask_TakeCover.h`
- `Source/SquadAI/Private/AI/StateTreeTasks/STTask_TakeCover.cpp`
- `Source/SquadAI/Public/AI/StateTreeTasks/STTask_EngageTarget.h`
- `Source/SquadAI/Private/AI/StateTreeTasks/STTask_EngageTarget.cpp`
- `Source/SquadAI/Public/AI/StateTreeTasks/STTask_LeaderPath.h`
- `Source/SquadAI/Private/AI/StateTreeTasks/STTask_LeaderPath.cpp`
- `Source/SquadAI/Private/AI/StateTreeTasks/STTask_FollowLeader.cpp`

### Characters / Components

- `Source/SquadAI/Public/Characters/SoldierCharacter.h`
- `Source/SquadAI/Private/Characters/SoldierCharacter.cpp`
- `Source/SquadAI/Private/Characters/AllySoldier.cpp`
- `Source/SquadAI/Private/Characters/EnemySoldier.cpp`
- `Source/SquadAI/Private/Characters/LeaderCharacter.cpp`
- `Source/SquadAI/Public/Components/AimComponent.h`
- `Source/SquadAI/Private/Components/AimComponent.cpp`
- `Source/SquadAI/Public/Components/WeaponComponent.h`
- `Source/SquadAI/Private/Components/WeaponComponent.cpp`
- `Source/SquadAI/Private/Components/HealthComponent.cpp`

### Cover System

- `Source/SquadAI/Public/CoverSystem/CoverScanner.h`
- `Source/SquadAI/Private/CoverSystem/CoverScanner.cpp`
- `Source/SquadAI/Public/CoverSystem/CoverSystemSubsystem.h`
- `Source/SquadAI/Private/CoverSystem/CoverSystemSubsystem.cpp`

### Tuning

- `Source/SquadAI/Public/SquadAITuning.h`

---

## 3. C++ Variables / Functions Added or Changed

### `ASoldierAIController`

Added/changed key variables:

```cpp
FVector LastMissionMoveGoal;
float LastMissionMoveRequestTime;

bool bEnableDirectCombatFallback;
float DirectCombatRange;
float DirectCombatBurstCooldown;
bool bAllowDirectCombatFallbackFire;
float DirectCombatFallbackFireRange;
float DirectCombatCloseRangeOverride;

bool bCoverQueryFinished;
bool bLastCoverQuerySucceeded;
FVector ActiveCoverMoveGoal;
bool bHasActiveCoverMoveGoal;
bool bActiveCoverMoveUsesRealCover;
float ActiveCoverMoveStartedTime;
float LastCoverRequestTime;
float LastTakeCoverCompletedTime;
float TakeCoverReentryCooldown;
```

Added/changed key functions:

```cpp
ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
void PollPerceptionForHostiles();
void PollRegistryForHostiles();
void UpsertPerceivedHostile(AActor* Actor, bool bCurrentlyVisible);
void RunDirectCombatFallback(float DeltaTime);
bool HasCombatThreat(float MinConfidence = 0.25f) const;
bool HasNearbyHostile(float Range) const;
```

### `ASoldierCharacter`

Added/changed animation variables:

```cpp
UAnimMontage* FireMontage;
UAnimMontage* DeathMontage;
UAnimMontage* HitReactMontage;
bool bWantsToFire;
float LastFireAnimTime;
float DeathMontageToRagdollDelay;
```

Added/changed function:

```cpp
void PlayFireAnimation();
```

Death now:

- stops firing/aiming,
- stops movement,
- attempts to stop StateTree logic,
- plays `DeathMontage` if assigned,
- delays ragdoll until after montage has time to show,
- ragdolls immediately only if montage missing/fails.

### `UAimComponent`

Added:

```cpp
bool bRotateWholeBodyWhenAiming;
float CombatTurnRate;
```

Purpose:

- If AnimBP aim offset is not fully implemented, AI still visibly rotates toward target.

### `UWeaponComponent`

Added logical AI muzzle settings:

```cpp
bool bUseLogicalAIMuzzle;
float LogicalMuzzleHeight;
float LogicalMuzzleForwardOffset;
```

Added robust hit helpers:

```cpp
FindHealthFromHitActor(...)
FindSoldierAlongShot(...)
```

Purpose:

- Damage soldiers even if their mesh/capsule does not block `Visibility`, as long as no world blocker is between shooter and target.
- Attached props/weapon cubes can still resolve to owner `HealthComponent`.

### `USquadAITuning`

Added settings under Project Settings:

```cpp
CombatDetectionRange
CombatCloseRangeOverride
MaxEngageRange
DirectFallbackFireRange
bAllowDirectFallbackFire

bAlliesInvincible
bLeaderInvincible

TacticalCoverSearchRadius
CoverQueryPartialResolveSeconds
CoverQueryTimeoutSeconds
CoverGroundTraceHalfHeight
bCoverUseLargeHeightFallbackTrace
```

---

## 4. Movement Fixes

### Enemy movement

`STTask_EnemyMission` was rewritten to use `AI->MoveToLocation()` directly instead of relying only on `UAITask_MoveTo`.

Fixes:

- Logs move result explicitly.
- Avoids `StopMovement()` spam.
- Tracks last mission goal on controller.
- Does not constantly re-request same path.
- Handles `AlreadyAtGoal`.

Useful log:

```text
EnemyMission: BP_EnemySoldier_C_1 MoveTo X=... -> RequestSuccessful / AlreadyAtGoal / Failed
```

### Leader movement / player wait bug

`STTask_LeaderPath` now checks actual `PathFollowingComponent` state. Previously, after leader stopped to wait for player, old `MoveTask` could remain but PathFollowing became idle, causing leader to never repath.

Fix:

- Repath if old MoveTask exists but path following is no longer moving.

### Leader combat interrupt

`STTask_LeaderPath` now detects combat threats and nearby hostiles. If StateTree fails to transition out of path-follow, the task becomes a tactical fallback:

- cancels mission movement,
- requests cover,
- moves to cover,
- aims while moving,
- allows fallback fire only when tactically allowed.

Useful log:

```text
LeaderPathCombat: BP_Leader_C_1 moving to cover X=...
```

### Ally follow interrupt

`STTask_FollowLeader` cancels follow movement when a combat threat exists.

---

## 5. Team / Perception / Threat Fixes

### Team ID

Controller now derives team from pawn faction in `OnPossess()`:

```cpp
Enemy = Team 1
Ally/Leader = Team 0
```

Also synced again in `ASoldierCharacter::BeginPlay()`.

### Attitude

Added explicit `GetTeamAttitudeTowards()` override.

### Perception fallback

Perception events were unreliable or filtered, so controller now also polls:

- current perceived actors,
- `USoldierRegistrySubsystem` nearby hostiles.

This fills `Memory` even if AIPerception event misses.

---

## 6. Cover System Fixes

### Scanner sample index bug

`FCoverScanner::SubmitAsyncProbes()` now receives local chunk sample index. This fixed cover samples not finalizing correctly.

### Landscape Z bug

Cover scanner now uses the **requesting pawn Z**, not fixed world Z or player Z.

```text
Trace band = Querier.Z ± CoverGroundTraceHalfHeight
```

### Partial query bug

Cover query no longer fails early just because first completed chunks have no cover. It waits for more chunks unless real cover is found or timeout occurs.

### Full chunk sample submission

Chunk scanner now submits all samples, rather than stopping after the trace budget hit.

### Cover scoring

Cover query scoring now considers:

- threat-facing score,
- body protection,
- peek-shot possibility,
- distance to querier,
- distance to mission goal.

Covers that cannot shoot are still valid, but lower priority.

### TakeCover rewrite

`STTask_TakeCover` now:

- requests real cover,
- falls back to tactical reposition only if needed,
- persists active cover move goal on controller to survive StateTree re-entry,
- handles `AlreadyAtGoal`,
- prevents immediate re-entry spam using `TakeCoverReentryCooldown`.

---

## 7. Weapon / Damage / Death Fixes

### Damage

Weapon now performs a soldier-registry ray hit before applying world-blocker hit. This fixed enemies not dying when character collision did not block Visibility.

### Health

Verbose health logs added:

```text
Health: BP_EnemySoldier_C_2 took 15.0 from BP_Leader_C_1, HP 85.0/100.0
Health: BP_EnemySoldier_C_2 died
```

### Death

Death now logs montage play result:

```text
DeathAnim: BP_EnemySoldier_C_2 Montage_Play(M_Death_Front) on AnimBP=ABP_CC_Soldier_Basic_C returned 1.000
```

If no montage or montage fails, ragdoll fallback is used.

### Fire animation

Fire now logs montage play result:

```text
FireAnim: BP_Leader_C_2 Montage_Play(M_Rifle_Fire_Stand) on AnimBP=ABP_CC_Soldier_Basic_C returned 1.000
```

This confirmed fire montage was playing; it was just visually subtle.

---

## 8. World Partition / Open World Notes

The map was converted/tested with World Partition to reduce landscape memory. VRAM improved significantly.

### Key observations

Memreport showed big landscape/texture costs:

```text
Terrain Heightmap ~608 MB
Terrain Weightmap ~640 MB
Texture2D total ~1.6 GB
```

Leader mesh polygons were not the main VRAM issue. Main runtime pressure came from:

- landscape height/weight maps,
- textures/materials,
- virtual shadow maps / render targets,
- CC high quality skin/wrinkle/sunglasses materials.

### World Partition setup notes

Important actors should be non-spatial / always loaded:

```text
QuestDirector
QuestMission
QuestRegistry-critical actors
Current QuestZone or active quest marker
Global managers
NavMeshBoundsVolume if needed for debugging
```

Landscape and foliage should generally remain spatially loaded to stream.

### Streaming source advice

Use streaming sources for:

```text
Player
Leader
Active Quest Zone
```

A `BP_QuestStreamingSource` can contain:

- World Partition Streaming Source component,
- Navigation Invoker component.

Set it:

```text
Is Spatially Loaded = false
Target State = Activated/Loaded
Loading Range ~60000-100000
```

### NavMesh after World Partition

After conversion, AI movement failed with:

```text
EnemyMission: ... MoveTo ... -> Failed
```

Likely causes:

- navmesh missing in streamed cells,
- NavMeshBoundsVolume unloaded,
- no Navigation Invokers,
- quest zone/goal cell loaded but navmesh not generated.

Recommended fixes:

- Mark `NavMeshBoundsVolume` non-spatial for testing.
- Use Runtime Generation = Dynamic while debugging.
- Add Navigation Invoker to player/leader/enemies/quest streaming source.
- Rebuild nav / press `P` to verify green navmesh.

### Landscape material warning

Repeated warnings:

```text
Material expects texture T_Soil_05_M to be Virtual
```

Fix by either:

- enabling Virtual Texture Streaming on that texture, or
- changing material sample type to non-virtual.

---

## 9. Character / Clothing Notes

Current character is imported via CC Auto Setup and outfit from Marvelous/CC pipeline.

### Body clipping through clothing

Problem:

- Some retained neck/body geometry clipped through clothing during animations.

Clarification:

- Cloth Paint is not for hiding body; it is for cloth simulation weights.
- Body clipping should be fixed by hiding/deleting body geometry under clothes.

Recommended fix:

- Duplicate skeletal mesh.
- Isolate body material section such as `Ga_Skin_Body_HQ_Inst`.
- Delete all hidden torso/shoulder/chest triangles except needed neck area.
- Use duplicate mesh in leader BP.

Alternative quick fix:

- Assign invisible masked material to body sections under clothes, if material sections allow.

---

## 10. Animation Pipeline Status

### Character pipeline

The character is **not MetaHuman**. It is a CC/Marvelous character imported into Unreal. Target skeleton is the CC skeleton.

### Packs

- Rifle Shooter Pro 4.27: base rifle locomotion/combat.
- Cover Animset Pro by Kubold: future cover-specific animations.

### Demo Blueprints

Do not use marketplace demo Blueprints in main project. Use only animation assets.

### Retargeting setup

Retargeter:

```text
RTG_RiflePro_To_CC
```

Important fixes:

- Set Retarget Root = pelvis on source and target IK Rigs.
- Remove one-bone Root/Pelvis chains.
- Remove finger/metacarpal chains for now.
- Disable/remove IK goals for arms during early retargeting.
- Source spine: `spine_01 -> spine_03`.
- Target spine: `spine_01 -> spine_04/spine_05`, whichever gives best result.
- Disable Post Process AnimBP during retarget preview if it interferes.

### Wrist twist issue

Problem:

- Left wrist collapsed/twisted during aiming animations.

Fix:

- End arm retarget chains at lowerarm instead of hand:

```text
LeftArm: upperarm_l -> lowerarm_l
RightArm: upperarm_r -> lowerarm_r
```

This prevents direct bad hand roll retargeting. Hand/finger grip will be fixed later with IK/grip pose.

---

## 11. Animation Stage Plan

### Stage 1 — Retargeting proof

Goal:

- Verify Rifle Pro animations retarget to CC character.

Status:

- Completed. Spine and wrist issues were fixed enough to continue.

### Stage 2 — Base rifle locomotion

Goal:

- Build base locomotion BlendSpaces and AnimBP states.

Status:

- Completed by user.
- Crouch staying issue was fixed by clearing crouch/cover state in `STTask_LeaderPath` when returning to mission path.

Stage 2 BlendSpaces:

```text
BS_Rifle_Relaxed_Stand
BS_Rifle_Aim_Stand
BS_Rifle_Crouch_Aim
```

Recommended animations used from Rifle Pro:

Relaxed:

```text
W2_Stand_Relaxed_Idle
W2_Walk_F/B/L/R_Loop
W2_Walk_FL/FR/BL_BkPd/BR_BkPd_Loop
W2_Jog_F/B/L/R_Loop
W2_Jog_FL/FR/BL_BkPd/BR_BkPd_Loop
W2_Run_F/L/R/FL/FR_Loop
```

Aim:

```text
W2_Stand_Aim_Idle
W2_Walk_Aim_F/B/L/R_Loop
W2_Walk_Aim_FL/FR/BL_BkPd/BR_BkPd_Loop
W2_Jog_Aim_F/B/L/R_Loop
W2_Jog_Aim_FL/FR/BL_BkPd/BR_BkPd_Loop
```

Crouch:

```text
W2_Crouch_Aim_Idle
W2_Crouch_Aim_Idle_v2
W2_CrouchWalk_Aim_F/B/L/R_Loop
W2_CrouchWalk_Aim_FL/FR/BL_BkPd/BR_BkPd_Loop
```

AnimBP variables created in editor (not C++):

```text
Speed : float
Direction : float
bIsCrouched : bool
bCombatReady : bool
bIsInCover : bool, for later
bIsDead : bool, for later
```

`bCombatReady` should be driven from:

```text
SoldierAIController::HasCombatThreat(0.25)
```

BlendSpace smoothing recommended:

```text
Speed smoothing ~0.20
Direction smoothing ~0.15
```

### Stage 3 — Fire / reload / death / hit reaction montages

Goal:

- Add montage support for firing and death.

Status:

- Fire montage confirmed working. It was subtle but `Montage_Play` returned success.
- Death montage issue fixed by adding FullBody slot and delaying ragdoll.

AnimGraph recommended structure:

```text
SM_RifleLocomotion -> Save Cached Pose CP_Locomotion

Use Cached Pose CP_Locomotion -> Layered Blend Per Bone Base Pose
Use Cached Pose CP_Locomotion -> Slot UpperBody -> Layered Blend Per Bone Blend Pose 0
Layered Blend Per Bone -> Slot FullBody -> Output Pose
```

Recommended Layered Blend settings:

```text
Branch Bone = spine_01 or spine_02
Blend Depth = 10 or 99 for testing
Mesh Space Rotation Blend = true
```

Montage slot usage:

```text
Fire / Reload / HitReact = UpperBody
Death = FullBody
```

Stage 3 Rifle Pro animations:

```text
W2_Stand_Fire_Burst
W2_Crouch_Fire_Burst
W2_Stand_Aim_Reload
W2_Crouch_Aim_Reload
W2_Stand_Relaxed_Death_F
W2_Stand_Relaxed_Death_B
W2_Stand_Relaxed_Death_L
W2_Stand_Relaxed_Death_R
```

User created an `UpperBody` slot under the default slot group. That is okay as long as the montage and AnimBP Slot node use exactly `UpperBody`.

FullBody slot must exist in AnimBP for death montages.

### Stage 4 — Cover Animset Pro integration (NEXT)

Goal:

- Use Kubold Cover Animset Pro animations for cover states.

Core cover animations to retarget first:

```text
CoverHi_CoverR
CoverHi_CoverL
CoverHi_AimR
CoverHi_AimL
CoverHi_ReloadR
CoverHi_ReloadL
CoverHi_AimR_Burst_Add
CoverHi_AimL_Burst_Add

CoverLo_CoverR
CoverLo_CoverL
CoverLo_AimR
CoverLo_AimL
CoverLo_AimU
CoverLo_ReloadR
CoverLo_ReloadL
CoverLo_AimR_Burst_Add
CoverLo_AimL_Burst_Add

CoverHi_WalkL
CoverHi_WalkR
CoverLo_WalkL
CoverLo_WalkR
```

Animation variables likely needed in AnimBP (some may need C++ later):

```text
bIsInCover : bool
bIsCrouchingInCover : bool
CoverSide : enum/int, Right/Left
CoverPeekType : enum/int, Side/Over
CoverMoveDirection : float, -1 left / +1 right
bIsReloading : bool
bIsPeeking : bool
```

C++ does not yet expose `CoverSide` or `CoverPeekType`. Add later.

### Stage 5 — Weapon IK / hand grip polish

Goal:

- Fix hand/finger placement on rifle.

Planned approach:

```text
Right hand: weapon socket / hand_r attachment
Left hand: Two Bone IK or Control Rig to rifle LeftHandIK socket
Fingers: static/additive rifle grip pose
Trigger finger: optional additive/control rig
```

Do not rely on retargeted finger chains.

### Stage 6 — Aim offsets

Goal:

- Use Rifle Pro and/or Cover Animset Pro aim offset poses for realistic aiming.

Do this only after base locomotion, montages, and cover states are stable.

### Stage 7 — Engage bad-shot recovery

Goal:

- If AI in cover keeps shooting landscape/sandbags and gets no valid hits for N seconds, leave Engage and request new cover/peek position.

---

## 12. Important Editor-Side Variables / Assets Not in C++

These names were introduced during setup and may exist only in editor assets:

### AnimBP variables

```text
Speed
Direction
bIsCrouched
bCombatReady
bIsInCover
bIsDead
bIsCrouchingInCover
bWantsToFire
AimYaw
AimPitch
```

Some are planned/future:

```text
CoverSide
CoverPeekType
CoverMoveDirection
bIsReloading
bIsPeeking
```

### Recommended BlendSpaces

```text
BS_Rifle_Relaxed_Stand
BS_Rifle_Aim_Stand
BS_Rifle_Crouch_Aim
```

### Recommended AnimBP

```text
ABP_CC_Soldier_Basic
```

or later rename to:

```text
ABP_CC_Soldier
```

### Recommended cached pose

```text
CP_Locomotion
```

### Recommended slots

```text
UpperBody
FullBody
```

### Recommended montages

```text
M_Rifle_Fire_Stand
M_Rifle_Fire_Crouch
M_Rifle_Reload_Stand
M_Rifle_Reload_Crouch
M_Death_Front
M_Death_Back
M_Death_Left
M_Death_Right
M_HitReact_Front
```

### Retargeting assets

```text
IK_RiflePro_Source
IK_CC_Target
RTG_RiflePro_To_CC
```

Future:

```text
IK_CoverPro_Source
RTG_CoverPro_To_CC
```

---

## 13. Known Remaining Issues

1. **Leader behavior should be retested** after World Partition and animation setup.
2. **Cover selection quality** still needs final tuning in some contexts.
3. **Engage bad-shot loop** needs blocked-shot detection.
4. **Hand/finger rifle grip** is not final; needs IK/grip pose.
5. **CC/Reallusion post-process AnimBP** may affect retarget preview; disable during retarget debugging.
6. **Animation pack demo Blueprints** should not be used in the main project.
7. **World Partition navmesh** requires careful setup with streaming sources and nav invokers.
8. **Landscape material warnings** about virtual textures should be cleaned.

---

## 14. Useful Debug Logs

Enable verbose logs:

```text
Log LogTemp Verbose
```

Useful log prefixes:

```text
EnemyMission:
TakeCover:
CoverQuery:
CombatFallback:
Health:
Weapon:
LeaderPathCombat:
FireAnim:
DeathAnim:
```

Example lines:

```text
FireAnim: BP_Leader_C_2 Montage_Play(M_Rifle_Fire_Stand) on AnimBP=ABP_CC_Soldier_Basic_C returned 1.000
DeathAnim: BP_EnemySoldier_C_2 Montage_Play(M_Death_Front) on AnimBP=ABP_CC_Soldier_Basic_C returned 1.000
Health: BP_EnemySoldier_C_2 took 15.0 from BP_Leader_C_1, HP 85.0/100.0
EnemyMission: BP_EnemySoldier_C_1 MoveTo X=... -> RequestSuccessful
CoverQuery: no cover for BP_... ConsideredPoints=...
```

---

## 15. Final Notes for Next Developer

- User prefers step-by-step instructions and low manual workload.
- User does not want manual cover markers all over the map; cover should remain procedural/automatic.
- User wants ally/leader invincible but inaccurate; enemies mortal.
- The current animation target is CC skeleton, not MetaHuman.
- Retargeted animations should be exported from the fixed retargeter after any retargeter change.
- If retargeter changes, overwrite exported animations so existing BlendSpaces keep references.
- Use in-place animations for AI locomotion because AI movement is driven by `MoveToLocation` / CharacterMovement.
- Avoid root motion for standard AI locomotion until the locomotion system is stable.

---

## 16. Git / Review Notes

Large modifications exist across AI, cover, weapon, health, tuning, and animation-adjacent code. Review with:

```bash
git status --short
git diff --stat
git diff
```

Before committing, consider splitting changes into commits:

1. AI movement/team/perception fixes.
2. Weapon/health/death fixes.
3. Cover scanner/system fixes.
4. Leader path/follow interrupts.
5. Animation debug/support changes.
6. Tuning settings additions.

---

## 17. Update — Stage 4C Cover Fire/Reload Montage Variant Support (2026-06-17)

Additional code support was added so designers can assign context-specific fire and reload montages without another C++ recompile.

### Files changed

```text
Source/SquadAI/Public/Characters/SoldierCharacter.h
Source/SquadAI/Private/Characters/SoldierCharacter.cpp
```

### New enums

```cpp
ESoldierCoverSide      // Right / Left
ESoldierCoverPeekType  // Side / Over
```

### New character state variables

```cpp
CoverSide
CoverPeekType
```

These are set automatically in `SetCurrentCover()` from `FCoverPoint.AvailableLeans`:

- prefer right lean if available,
- else left lean,
- low cover with over lean sets `CoverPeekType = Over`.

### New fire montage variables

```cpp
FireMontage_Crouch
FireMontage_CoverHighRight
FireMontage_CoverHighLeft
FireMontage_CoverLowRight
FireMontage_CoverLowLeft
FireMontage_CoverLowOver
```

### New reload montage variables

```cpp
ReloadMontage
ReloadMontage_Crouch
ReloadMontage_CoverHighRight
ReloadMontage_CoverHighLeft
ReloadMontage_CoverLowRight
ReloadMontage_CoverLowLeft
ReloadMontage_CoverLowOver
```

### New helper functions

```cpp
IsCurrentCoverLow()
IsCoverPeekOver()
SelectFireMontage()
SelectReloadMontage()
PlayReloadAnimation()
```

`PlayFireAnimation()` now calls `SelectFireMontage()` and chooses the best assigned montage based on cover state. If a specific montage is missing, it falls back to crouch/default fire montage.

### New random montage arrays

```cpp
HitReactMontages
DeathMontages
```

If populated, hit/death code chooses a random montage from the array. Existing single `HitReactMontage` and `DeathMontage` still work as fallback.

### Extra documentation

See also:

```text
ANIMATION_STAGE4C_MONTAGE_VARIANTS.md
```

### Designer setup reminder

After recompiling, assign in each soldier Blueprint as needed:

```text
FireMontage = M_Rifle_Fire_Stand
FireMontage_Crouch = M_Rifle_Fire_Crouch
FireMontage_CoverHighRight = M_Fire_CoverHi_R
FireMontage_CoverHighLeft = M_Fire_CoverHi_L
FireMontage_CoverLowRight = M_Fire_CoverLo_R
FireMontage_CoverLowLeft = M_Fire_CoverLo_L
FireMontage_CoverLowOver = M_Fire_CoverLo_Over or fallback M_Rifle_Fire_Crouch
```

All fire/reload/hit montages should use the `UpperBody` slot. Death montages should use the `FullBody` slot.
