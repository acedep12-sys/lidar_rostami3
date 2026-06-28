# Animation Stage 4 Cover State Fix Notes

Date: 2026-06-17

## Problem

During Stage 4 cover animation setup, characters entered `CoverLow_Hidden` / `CoverHigh_Hidden` animations while still physically moving toward a cover point. This caused:

- low-cover hidden pose sliding across the ground,
- high-cover pose playing from far away before reaching the wall/sandbag,
- unrealistic movement when moving from one cover side/point to another.

## Root Cause

`ASoldierCharacter::SetCurrentCover()` was setting:

```cpp
bIsInCover = true;
```

as soon as a cover point was found/reserved.

That was too early. At that moment, the pawn may still be far away and moving toward the cover. The AnimBP reads `bIsInCover`, so it immediately switched into cover idle/hidden animations while the movement component continued moving the pawn.

## Fix Applied

File changed:

```text
Source/SquadAI/Private/Characters/SoldierCharacter.cpp
```

Changed `SetCurrentCover()` so it only stores the selected cover point and does **not** set `bIsInCover`.

New behavior:

```cpp
void ASoldierCharacter::SetCurrentCover(const FCoverPoint& Cover)
{
    // Stores selected/reserved cover point only.
    // Actual cover state is set only after arrival via SetCoverState().
    CurrentCoverPoint = Cover;
}
```

`bIsInCover` should now become true only when `SetCoverState(true, ...)` is called after the pawn reaches cover in `STTask_TakeCover`.

## Expected Result

- While moving to cover: AnimBP remains in normal tactical movement / crouch movement, not cover idle.
- After reaching cover: `SetCoverState(true, ...)` switches AnimBP into cover state.
- High cover animation no longer starts while far away from the cover.
- Low cover hidden pose no longer slides across the terrain while moving.

## AnimBP Design Reminder

Use these variables from `ASoldierCharacter`:

```text
bIsInCover
bIsCrouchingInCover
CurrentCoverPoint.Height
```

Interpretation:

```text
bIsInCover == false
    → not actually at cover yet; use locomotion/tactical movement.

bIsInCover == true && bIsCrouchingInCover == true
    → hidden/crouched behind cover.

bIsInCover == true && bIsCrouchingInCover == false
    → peeking/standing/engaging from cover.
```
