// STTask_EngageTarget.cpp — Suppression-gated peek/fire/cooldown
#include "AI/StateTreeTasks/STTask_EngageTarget.h"
#include "StateTreeExecutionContext.h"
#include "AI/SoldierAIController.h"
#include "Characters/SoldierCharacter.h"
#include "Characters/EnemySoldier.h"
#include "Components/WeaponComponent.h"
#include "Components/AimComponent.h"
#include "SquadAITuning.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "NativeGameplayTags.h"

const UStruct* FSTTask_EngageTarget::GetInstanceDataType() const
{
	return FSTTask_EngageTargetData::StaticStruct();
}

EStateTreeRunStatus FSTTask_EngageTarget::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	auto& D = Context.GetInstanceData<FSTTask_EngageTargetData>(*this);
	D.Phase = EEngagePhase::Waiting;
	D.PhaseTimer = 0.f;
	D.CyclesCompleted = 0;
	D.BlockedShotCount = 0;
	D.LastShotCheckTime = 0.f;
	D.CurrentPeekDuration = FMath::RandRange(PeekDurationMin, PeekDurationMax);
	D.CurrentCooldownDuration = FMath::RandRange(CooldownMin, CooldownMax);

	// Start crouched in cover
	ASoldierAIController* AI = Cast<ASoldierAIController>(Context.GetOwner());
	if (AI && AI->GetPawn())
	{
		if (ASoldierCharacter* S = Cast<ASoldierCharacter>(AI->GetPawn()))
		{
			S->SetCoverState(true, true);
		}
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTask_EngageTarget::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	auto& D = Context.GetInstanceData<FSTTask_EngageTargetData>(*this);

	ASoldierAIController* AI = Cast<ASoldierAIController>(Context.GetOwner());
	if (!AI || !AI->GetPawn()) return EStateTreeRunStatus::Failed;

	ASoldierCharacter* Soldier = Cast<ASoldierCharacter>(AI->GetPawn());
	if (!Soldier) return EStateTreeRunStatus::Failed;

	// No threat / no clear shot? Succeed out so StateTree can reposition instead of
	// shooting red debug lines into hills or through blocked terrain.
	float Confidence = 0.f;
	AActor* Threat = AI->GetPrimaryThreat(Confidence);
	const float EffectiveMaxEngageRange = FMath::Max(MaxEngageRange, USquadAITuning::Get()->MaxEngageRange);
	const float ThreatDist = Threat ? FVector::Dist(AI->GetPawn()->GetActorLocation(), Threat->GetActorLocation()) : FLT_MAX;
	if (!Threat || Confidence < 0.2f || ThreatDist > EffectiveMaxEngageRange || !AI->HasLineOfSightTo(Threat))
	{
		if (Soldier->WeaponComp) Soldier->WeaponComp->StopFiring();
		if (Soldier->AimComp) Soldier->AimComp->ClearAimTarget();
		return EStateTreeRunStatus::Succeeded;
	}

	// Get suppression threshold from enemy config
	float SupThreshold = 0.6f;
	if (AEnemySoldier* Enemy = Cast<AEnemySoldier>(AI->GetPawn()))
	{
		SupThreshold = Enemy->SuppressionThreshold;
	}

	D.PhaseTimer += DeltaTime;

	switch (D.Phase)
	{
	// ---- WAITING (behind cover) ----
	case EEngagePhase::Waiting:
	{
		// Suppression gate — refuse to peek while suppressed
		if (AI->GetSuppression() > SupThreshold)
		{
			D.PhaseTimer = 0.f; // Reset timer — stay hidden
			return EStateTreeRunStatus::Running;
		}

		// Wait before peeking (randomized cooldown duration)
		if (D.PhaseTimer >= D.CurrentCooldownDuration)
		{
			// Peek!
			D.Phase = EEngagePhase::Peeking;
			D.PhaseTimer = 0.f;

			Soldier->SetCoverState(true, false); // Stand up in cover

			// Aim at threat
			if (Soldier->AimComp)
			{
				Soldier->AimComp->SetAimTarget(Threat->GetActorLocation());
			}

			// Tell Lyra to Fire the Weapon
			FGameplayEventData Payload;
			Payload.Instigator = Soldier;
			Payload.Target = Threat;
			
			// Send the exact Gameplay Tag Lyra uses to pull the trigger
			FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(FName("InputTag.Weapon.Fire"));
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Soldier, FireTag, Payload);
		}
		break;
	}

	// ---- PEEKING (exposed, firing) ----
	case EEngagePhase::Peeking:
	{
		// Update aim to track moving threat
		if (Soldier->AimComp && Threat)
		{
			Soldier->AimComp->SetAimTarget(Threat->GetActorLocation());
		}
		
		// Optional logic to detect if we're constantly shooting into a sandbag or landscape.
		// If so, increment a counter. If it reaches MaxBlockedShots, we force a Succeeded out so the AI finds new cover.
		if (Soldier->WeaponComp && Soldier->WeaponComp->CurrentState == EWeaponState::Firing)
		{
			const float CurrentTime = Soldier->GetWorld()->GetTimeSeconds();
			if (CurrentTime - D.LastShotCheckTime > 0.5f) // Poll every 0.5s while firing
			{
				D.LastShotCheckTime = CurrentTime;
				
				FTransform MuzzleWorld;
				if (Soldier->GetMuzzleWorldTransform(MuzzleWorld)) 
				{
					FVector MuzzleLoc = MuzzleWorld.GetLocation();
					// Simple raycast to threat
					FVector Dir = (Threat->GetActorLocation() - MuzzleLoc).GetSafeNormal();
					FHitResult HitResult;
					FCollisionQueryParams Params;
					Params.AddIgnoredActor(Soldier);
					Params.AddIgnoredActor(Threat);
					
					bool bHit = Soldier->GetWorld()->LineTraceSingleByChannel(HitResult, MuzzleLoc, Threat->GetActorLocation(), ECC_Visibility, Params);
					if (bHit && !Cast<ASoldierCharacter>(HitResult.GetActor())) // Hit something static
					{
						D.BlockedShotCount++;
						if (D.BlockedShotCount >= MaxBlockedShots)
						{
							Soldier->WeaponComp->StopFiring();
							Soldier->AimComp->ClearAimTarget();
							Soldier->SetCoverState(true, true);
							return EStateTreeRunStatus::Succeeded; // Exit to trigger reposition
						}
					}
					else 
					{
						D.BlockedShotCount = 0; // Clear on clear shot
					}
				}
			}
		}

		// Do NOT call StartBurst every tick here. StartBurst resets the burst shot count
		// and restarts the firing montage, which creates visible aim/upper-body snapping
		// between shots. The burst is started once when entering Peeking; while peeking we
		// only keep the aim target updated.

		// Time's up or suppressed mid-peek → duck
		if (D.PhaseTimer >= D.CurrentPeekDuration || AI->GetSuppression() > SupThreshold * 1.2f)
		{
			D.Phase = EEngagePhase::Cooldown;
			D.PhaseTimer = 0.f;

			// Duck back
			if (Soldier->WeaponComp) Soldier->WeaponComp->StopFiring();
			Soldier->SetCoverState(true, true);
			Soldier->AimComp->ClearAimTarget();
			D.CyclesCompleted++;

			// Randomize durations for next cycle (each peek feels different)
			D.CurrentPeekDuration = FMath::RandRange(PeekDurationMin, PeekDurationMax);
			D.CurrentCooldownDuration = FMath::RandRange(CooldownMin, CooldownMax);
		}
		break;
	}

	// ---- COOLDOWN (behind cover, waiting) ----
	case EEngagePhase::Cooldown:
	{
		if (D.PhaseTimer >= D.CurrentCooldownDuration)
		{
			if (D.CyclesCompleted >= MaxCycles)
			{
				return EStateTreeRunStatus::Succeeded; // Done — StateTree decides what's next
			}

			D.Phase = EEngagePhase::Waiting;
			D.PhaseTimer = 0.f;
		}
		break;
	}
	}

	return EStateTreeRunStatus::Running;
}

void FSTTask_EngageTarget::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	ASoldierAIController* AI = Cast<ASoldierAIController>(Context.GetOwner());
	if (AI && AI->GetPawn())
	{
		if (ASoldierCharacter* S = Cast<ASoldierCharacter>(AI->GetPawn()))
		{
			if (S->WeaponComp) S->WeaponComp->StopFiring();
			S->SetCoverState(true, true);
			if (S->AimComp) S->AimComp->ClearAimTarget();
		}
	}
}
