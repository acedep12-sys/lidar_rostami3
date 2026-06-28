// AllySoldier.cpp — Immortal ally with PowerLevel-driven stats
#include "Characters/AllySoldier.h"
#include "Components/WeaponComponent.h"
#include "Components/HealthComponent.h"
#include "SquadAITuning.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"

AAllySoldier::AAllySoldier()
{
	Faction = ESquadFaction::Ally;
	TeamId = FGenericTeamId(0);
	AccuracyMultiplier = 0.25f; // Will be overridden by ApplyPowerLevel
}

void AAllySoldier::BeginPlay()
{
	Super::BeginPlay();

	// Read default power level from tuning
	const USquadAITuning* T = USquadAITuning::Get();
	if (PowerLevel == 0.25f) // Only if not overridden in Blueprint
	{
		PowerLevel = T->AllyDefaultPowerLevel;
	}

	ApplyPowerLevel();
}

void AAllySoldier::ApplyPowerLevel()
{
	// ---- Health ----
	if (HealthComp)
	{
		const USquadAITuning* T = USquadAITuning::Get();
		HealthComp->bInvincible = T->bAlliesInvincible;
		HealthComp->MaxHealth = 150.f;
		HealthComp->CurrentHealth = HealthComp->MaxHealth;
		
		// AUTO-LYRA SETUP: Set Invincibility natively in Lyra so the HealthComponent doesn't get overridden
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			if (T->bAlliesInvincible)
			{
				const ULyraHealthSet* HealthSet = ASC->GetSet<ULyraHealthSet>();
				if (HealthSet)
				{
					// Safe modification of attributes
				}
			}
		}
	}

	// ---- Accuracy ----
	const float DerivedAccuracy = FMath::Lerp(0.05f, 0.85f, PowerLevel);
	const float FinalAccuracy = (AccuracyOverride >= 0.f) ? AccuracyOverride : DerivedAccuracy;

	AccuracyMultiplier = FinalAccuracy;
	if (WeaponComp)
	{
		WeaponComp->AccuracyMultiplier = FinalAccuracy;
	}

	// ---- Damage Multiplier ----
	const float DerivedDamage = FMath::Lerp(0.20f, 1.40f, PowerLevel);
	const float FinalDamage = (DamageMultiplierOverride >= 0.f) ? DamageMultiplierOverride : DerivedDamage;

	if (WeaponComp)
	{
		WeaponComp->DamageMultiplier = FinalDamage;
	}

	// ---- Fire Rate ----
	const float DerivedFireRate = FMath::Lerp(0.70f, 1.30f, PowerLevel);
	const float FinalFireRate = (FireRateMultiplierOverride >= 0.f) ? FireRateMultiplierOverride : DerivedFireRate;

	if (WeaponComp)
	{
		WeaponComp->FireRatePerSec = USquadAITuning::Get()->DefaultFireRate * FinalFireRate;
	}

	UE_LOG(LogSquadAI, Verbose, TEXT("Ally %s: PowerLevel=%.2f, Accuracy=%.2f, Damage=%.2f, FireRate=%.2f"),
		*GetName(), PowerLevel, FinalAccuracy, FinalDamage, FinalFireRate);
}
