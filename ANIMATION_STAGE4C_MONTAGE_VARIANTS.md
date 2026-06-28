# Animation Stage 4C — Cover Fire/Reload Montage Variant Support

Date: 2026-06-17

## Purpose

Stage 4C adds code support for context-aware animation montages so the same C++ shooting call can play different animations depending on whether the soldier is standing, crouched, in high cover, in low cover, or peeking over low cover.

This lets designers assign Cover Animset Pro montages without requiring another C++ recompile.

## Files Changed

```text
Source/SquadAI/Public/Characters/SoldierCharacter.h
Source/SquadAI/Private/Characters/SoldierCharacter.cpp
```

## New Cover Animation Enums

Added in `SoldierCharacter.h`:

```cpp
UENUM(BlueprintType)
enum class ESoldierCoverSide : uint8
{
    Right,
    Left
};

UENUM(BlueprintType)
enum class ESoldierCoverPeekType : uint8
{
    Side,
    Over
};
```

## New Cover State Variables

Added to `ASoldierCharacter`:

```cpp
ESoldierCoverSide CoverSide = ESoldierCoverSide::Right;
ESoldierCoverPeekType CoverPeekType = ESoldierCoverPeekType::Side;
```

`SetCurrentCover()` now picks default values from `FCoverPoint.AvailableLeans`:

- Prefer `Right` if available.
- Else prefer `Left`.
- For low cover with `Over` lean available, set `CoverPeekType = Over`.
- Otherwise `CoverPeekType = Side`.

## New Fire Montage Variables

Assign these in `BP_Leader`, `BP_EnemySoldier`, `BP_AllySoldier` as desired:

```cpp
FireMontage                  // fallback/default standing fire
FireMontage_Crouch
FireMontage_CoverHighRight
FireMontage_CoverHighLeft
FireMontage_CoverLowRight
FireMontage_CoverLowLeft
FireMontage_CoverLowOver
```

## New Reload Montage Variables

Prepared for the future reload system:

```cpp
ReloadMontage                // fallback/default standing reload
ReloadMontage_Crouch
ReloadMontage_CoverHighRight
ReloadMontage_CoverHighLeft
ReloadMontage_CoverLowRight
ReloadMontage_CoverLowLeft
ReloadMontage_CoverLowOver
```

## New Helper Functions

```cpp
bool IsCurrentCoverLow() const;
bool IsCoverPeekOver() const;
UAnimMontage* SelectFireMontage() const;
UAnimMontage* SelectReloadMontage() const;
void PlayReloadAnimation();
```

## Fire Montage Selection Logic

`PlayFireAnimation()` now calls `SelectFireMontage()`.

Selection priority:

1. If in low cover and peek type is over and `FireMontage_CoverLowOver` is set, use that.
2. If in low cover and side is left/right, use matching low-cover montage if assigned.
3. If in high/partial cover, use matching high-cover left/right montage if assigned.
4. If crouched and `FireMontage_Crouch` is set, use crouch montage.
5. Else use default `FireMontage`.

Debug log now prints chosen montage and context:

```text
FireAnim: BP_... Montage_Play(M_Fire_CoverLowOver) ... InCover=1 Low=1 Side=Right Peek=Over
```

## Death / Hit React Improvements

Added arrays:

```cpp
TArray<UAnimMontage*> HitReactMontages;
TArray<UAnimMontage*> DeathMontages;
```

If arrays are populated, code picks a random montage from them. Existing single montage variables still work as fallback:

```cpp
HitReactMontage
DeathMontage
```

## What Designers Should Assign Now

For Stage 4C, after creating/retargeting Cover Animset Pro montages, assign:

```text
FireMontage = M_Rifle_Fire_Stand
FireMontage_Crouch = M_Rifle_Fire_Crouch
FireMontage_CoverHighRight = M_Fire_CoverHi_R
FireMontage_CoverHighLeft = M_Fire_CoverHi_L
FireMontage_CoverLowRight = M_Fire_CoverLo_R
FireMontage_CoverLowLeft = M_Fire_CoverLo_L
FireMontage_CoverLowOver = M_Fire_CoverLo_Over or fallback M_Rifle_Fire_Crouch
```

Reload montages can be assigned now even if reload gameplay is added later:

```text
ReloadMontage = M_Rifle_Reload_Stand
ReloadMontage_Crouch = M_Rifle_Reload_Crouch
ReloadMontage_CoverHighRight = M_Reload_CoverHi_R
ReloadMontage_CoverHighLeft = M_Reload_CoverHi_L
ReloadMontage_CoverLowRight = M_Reload_CoverLo_R
ReloadMontage_CoverLowLeft = M_Reload_CoverLo_L
ReloadMontage_CoverLowOver = M_Reload_CoverLo_R or a custom over-cover reload
```

## Notes

- `*_Burst_Add` animations from Cover Animset Pro may be additive. If they look invisible/weird as normal montages, use Rifle Pro fire montages as cover fire placeholders and add additive recoil later.
- Reload montages are normal and should be easier to verify.
- All fire/reload/hit montages should use the `UpperBody` slot.
- Death montages should use the `FullBody` slot.
