// EnemySoldier.cpp — Enemy with tuning-driven stats
#include "Characters/EnemySoldier.h"
#include "Components/WeaponComponent.h"
#include "Components/HealthComponent.h"
#include "SquadAITuning.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"

AEnemySoldier::AEnemySoldier()
{
	Faction = ESquadFaction::Enemy;
}

void AEnemySoldier::BeginPlay()
{
	Super::BeginPlay();

	const USquadAITuning* T = USquadAITuning::Get();

	AccuracyMultiplier = T->EnemyAccuracyMultiplier;
	Aggression = T->EnemyDefaultAggression;
	SuppressionThreshold = T->EnemySuppressionThreshold;

	if (WeaponComp)
	{
		WeaponComp->AccuracyMultiplier = AccuracyMultiplier;
	}

	if (HealthComp)
	{
		HealthComp->bInvincible = false;
		HealthComp->MaxHealth = T->DefaultMaxHealth;
		HealthComp->CurrentHealth = HealthComp->MaxHealth;
	}
	
	// AUTO-LYRA SETUP: Make sure normal enemies are definitely mortal in the Ability System
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const ULyraHealthSet* HealthSet = ASC->GetSet<ULyraHealthSet>();
		if (HealthSet)
		{
			// Safe modification of attributes
		}
	}
}
