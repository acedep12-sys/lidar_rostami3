#include "AI/SoldierAIController.h"
#include "Characters/SoldierCharacter.h"
#include "CoverSystem/CoverSystemSubsystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/AimComponent.h"
#include "Components/WeaponComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "SquadAITuning.h"
#include "Performance/SharedThreatMemory.h"
#include "Performance/AISignificanceManager.h"
#include "Performance/SoldierRegistrySubsystem.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayTasksComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASoldierAIController::ASoldierAIController(const FObjectInitializer& OI)
	: Super(OI.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	bWantsPlayerState = true; // Crucial for Lyra Ability System and Team integration
	
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SetPerceptionComponent(*PerceptionComp);
	StateTreeComp = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTree"));
	TasksComp = CreateDefaultSubobject<UGameplayTasksComponent>(TEXT("GameplayTasks"));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; 
	TeamId = FGenericTeamId(255);
}

void ASoldierAIController::BeginPlay()
{
	Super::BeginPlay();
	const USquadAITuning* T = USquadAITuning::Get();
	MemoryDecayRate = T->MemoryDecayPerSecond;
	SuppressionDecayPerSec = T->SuppressionDecayPerSecond;
	DirectCombatRange = T->CombatDetectionRange;
	DirectCombatCloseRangeOverride = T->CombatCloseRangeOverride;
	DirectCombatFallbackFireRange = T->DirectFallbackFireRange;
	bAllowDirectCombatFallbackFire = T->bAllowDirectFallbackFire;

	// Configure perception after the listener has been registered. Calling ConfigureSense
	// too early produces "Listener must have a valid id" and can make perception events unreliable.
	FTimerHandle PerceptionSetupHandle;
	GetWorldTimerManager().SetTimer(PerceptionSetupHandle, this, &ASoldierAIController::SetupPerception, 1.0f, false);
}

void ASoldierAIController::OnPossess(APawn* InPawn)
{
	// AUTO-LYRA SETUP: If we possess a Lyra character, force Lyra to give it a PlayerState immediately
	// This prevents the "Invalid Ability System Component" errors without needing a Bot Spawner.
	if (ALyraCharacter* LyraEnemy = Cast<ALyraCharacter>(InPawn))
	{
		// Note: DispatchInitializationStartup removed. Lyra handles PlayerState init natively when GameMode assigns bots.
	}

	// Do NOT copy the pawn's cached TeamId here. For placed AI, possession can happen
	// before BeginPlay, so ASoldierCharacter::BeginPlay may not have populated TeamId yet.
	// Faction is already initialized from C++/Blueprint defaults, so derive the team from it.
	if (ASoldierCharacter* Soldier = Cast<ASoldierCharacter>(InPawn))
	{
		SetGenericTeamId(Soldier->Faction == ESquadFaction::Enemy ? FGenericTeamId(1) : FGenericTeamId(0));
		Soldier->SetGenericTeamId(GetGenericTeamId());
	}

	Super::OnPossess(InPawn);
	if (StateTreeComp) StateTreeComp->RestartLogic();
	if (PerceptionComp) PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ASoldierAIController::OnPerceptionUpdated);
}

ETeamAttitude::Type ASoldierAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	const IGenericTeamAgentInterface* OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(&Other);

	// If the perceived actor is a pawn, its controller may be the actual team agent.
	if (!OtherTeamAgent)
	{
		if (const APawn* OtherPawn = Cast<const APawn>(&Other))
		{
			OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(OtherPawn->GetController());
		}
	}

	if (!OtherTeamAgent)
	{
		return ETeamAttitude::Neutral;
	}

	const FGenericTeamId OtherTeamId = OtherTeamAgent->GetGenericTeamId();
	if (TeamId == FGenericTeamId::NoTeam || OtherTeamId == FGenericTeamId::NoTeam)
	{
		return ETeamAttitude::Neutral;
	}

	return TeamId == OtherTeamId ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

void ASoldierAIController::OnUnPossess()
{
	if (PerceptionComp) PerceptionComp->OnTargetPerceptionUpdated.RemoveDynamic(this, &ASoldierAIController::OnPerceptionUpdated);
	Super::OnUnPossess();
}

void ASoldierAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ASoldierCharacter* Soldier = GetSoldierCharacter();
	const float CoverDecayMult = (Soldier && Soldier->bIsInCover) ? 0.5f : 1.f;
	Suppression = FMath::Max(0.f, Suppression - SuppressionDecayPerSec * DeltaTime * CoverDecayMult);
	UpdateMemory(DeltaTime);
	PollPerceptionForHostiles();
	PollRegistryForHostiles();
	RunDirectCombatFallback(DeltaTime);
}

void ASoldierAIController::SetupPerception()
{
	if (!PerceptionComp) return;
	const USquadAITuning* T = USquadAITuning::Get();
	SightConfig = NewObject<UAISenseConfig_Sight>(this);
	SightConfig->SightRadius = T->SightRadius;
	SightConfig->LoseSightRadius = T->LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = T->PeripheralVisionDegrees;
	SightConfig->DetectionByAffiliation.bDetectEnemies = SightConfig->DetectionByAffiliation.bDetectNeutrals = SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	PerceptionComp->ConfigureSense(*SightConfig);
	HearingConfig = NewObject<UAISenseConfig_Hearing>(this);
	HearingConfig->HearingRange = T->HearingRange;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = HearingConfig->DetectionByAffiliation.bDetectNeutrals = HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	PerceptionComp->ConfigureSense(*HearingConfig);
	DamageConfig = NewObject<UAISenseConfig_Damage>(this);
	PerceptionComp->ConfigureSense(*DamageConfig);
}

void ASoldierAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor || !Cast<APawn>(Actor)) return;
	bool bIsAlive = true;
	if (ASoldierCharacter* S = Cast<ASoldierCharacter>(Actor)) bIsAlive = S->IsAlive();
	if (!bIsAlive) return;

	// Use this controller's team attitude check.
	if (GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile) return;

	UpsertPerceivedHostile(Actor, Stimulus.WasSuccessfullySensed());
}

void ASoldierAIController::UpdateMemory(float DeltaTime)
{
	for (int32 i = Memory.Num() - 1; i >= 0; --i) {
		FPerceivedTarget& T = Memory[i];
		if (!T.Actor.IsValid() || (Cast<ASoldierCharacter>(T.Actor.Get()) && !Cast<ASoldierCharacter>(T.Actor.Get())->IsAlive())) {
			Memory.RemoveAtSwap(i); continue;
		}
		if (!T.bCurrentlyVisible) {
			T.Confidence -= MemoryDecayRate * DeltaTime;
			if (T.Confidence <= 0.f) Memory.RemoveAtSwap(i);
		} else T.LastKnownLocation = T.Actor->GetActorLocation();
	}
}

void ASoldierAIController::UpsertPerceivedHostile(AActor* Actor, bool bCurrentlyVisible)
{
	if (!Actor || !GetPawn()) return;

	FPerceivedTarget* Existing = Memory.FindByPredicate([Actor](const FPerceivedTarget& T) { return T.Actor.Get() == Actor; });
	if (Existing)
	{
		Existing->LastKnownLocation = Actor->GetActorLocation();
		Existing->Confidence = bCurrentlyVisible ? 1.f : Existing->Confidence;
		Existing->bCurrentlyVisible = bCurrentlyVisible;
	}
	else if (bCurrentlyVisible)
	{
		FPerceivedTarget NewTarget;
		NewTarget.Actor = Actor;
		NewTarget.LastKnownLocation = Actor->GetActorLocation();
		NewTarget.Confidence = 1.f;
		NewTarget.bCurrentlyVisible = true;
		Memory.Add(NewTarget);
	}
}

void ASoldierAIController::PollPerceptionForHostiles()
{
	if (!PerceptionComp || !GetPawn()) return;

	TArray<AActor*> VisibleActors;
	PerceptionComp->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), VisibleActors);

	for (AActor* Actor : VisibleActors)
	{
		if (!Actor || Actor == GetPawn() || !Cast<APawn>(Actor)) continue;

		if (ASoldierCharacter* S = Cast<ASoldierCharacter>(Actor))
		{
			if (!S->IsAlive()) continue;
		}

		if (GetTeamAttitudeTowards(*Actor) == ETeamAttitude::Hostile)
		{
			UpsertPerceivedHostile(Actor, true);
		}
	}
}

void ASoldierAIController::PollRegistryForHostiles()
{
	if (!GetPawn()) return;

	USoldierRegistrySubsystem* Registry = GetWorld()->GetSubsystem<USoldierRegistrySubsystem>();
	if (!Registry) return;

	TArray<ASoldierCharacter*> Nearby = Registry->QueryRadius(GetPawn()->GetActorLocation(), DirectCombatRange);
	for (ASoldierCharacter* Other : Nearby)
	{
		if (!Other || Other == GetPawn() || !Other->IsAlive()) continue;

		const bool bDifferentTeam = Other->GetGenericTeamId().GetId() != GetGenericTeamId().GetId();
		if (!bDifferentTeam) continue;

		// Registry is the cheap spatial broad phase. Line of sight is the narrow phase.
		// The close-range override prevents soldiers standing idle face-to-face if the
		// Visibility trace is blocked by attachments/capsules/terrain seams.
		const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Other->GetActorLocation());
		if (HasLineOfSightTo(Other) || Dist <= DirectCombatCloseRangeOverride)
		{
			UpsertPerceivedHostile(Other, true);
		}
	}
}

void ASoldierAIController::RunDirectCombatFallback(float DeltaTime)
{
	if (!bEnableDirectCombatFallback || !GetPawn()) return;

	ASoldierCharacter* Soldier = GetSoldierCharacter();
	if (!Soldier) return;

	float Confidence = 0.f;
	AActor* Threat = GetPrimaryThreat(Confidence);
	if (!Threat || Confidence < 0.25f)
	{
		if (Soldier->AimComp) Soldier->AimComp->ClearAimTarget();
		if (Soldier->WeaponComp) Soldier->WeaponComp->StopFiring();
		return;
	}

	if (const ASoldierCharacter* ThreatSoldier = Cast<ASoldierCharacter>(Threat))
	{
		if (!ThreatSoldier->IsAlive())
		{
			if (Soldier->AimComp) Soldier->AimComp->ClearAimTarget();
			if (Soldier->WeaponComp) Soldier->WeaponComp->StopFiring();
			return;
		}
	}

	const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Threat->GetActorLocation());
	if (Dist > DirectCombatRange) return;

	const bool bHasCombatLOS = HasLineOfSightTo(Threat) || Dist <= DirectCombatCloseRangeOverride;
	if (!bHasCombatLOS)
	{
		if (Soldier->AimComp) Soldier->AimComp->ClearAimTarget();
		if (Soldier->WeaponComp) Soldier->WeaponComp->StopFiring();
		return;
	}

	const FVector AimLoc = Threat->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	if (Soldier->AimComp)
	{
		Soldier->AimComp->SetAimTarget(AimLoc);
	}

	// If this AI is following a mission/formation path and sees a hostile, stop that
	// path immediately. Cover movement is allowed to continue because it sets
	// bHasActiveCoverMoveGoal. This prevents the leader from walking straight into enemies.
	if (!bHasActiveCoverMoveGoal)
	{
		if (UPathFollowingComponent* PFC = GetPathFollowingComponent())
		{
			if (PFC->GetStatus() == EPathFollowingStatus::Moving)
			{
				StopMovement();
			}
		}
	}

	// Fallback fire is only allowed when the AI is actively moving to cover, already in
	// cover, or close enough for a direct fight. This prevents long-range open-field
	// standing fire while still allowing "shoot while moving to cover" behavior.
	const bool bTacticallyAllowedToFallbackFire = bHasActiveCoverMoveGoal || bHasCover || Dist <= 2800.f;
	if (!bAllowDirectCombatFallbackFire || Dist > DirectCombatFallbackFireRange || !bTacticallyAllowedToFallbackFire)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Soldier->WeaponComp && Now - LastDirectCombatBurstTime >= DirectCombatBurstCooldown)
	{
		Soldier->WeaponComp->StartBurst(AimLoc);
		LastDirectCombatBurstTime = Now;

		UE_LOG(LogTemp, Verbose, TEXT("CombatFallback: %s engaging %s confidence=%.2f dist=%.0f"),
			*GetNameSafe(GetPawn()), *GetNameSafe(Threat), Confidence, Dist);
	}
}

AActor* ASoldierAIController::GetPrimaryThreat(float& OutConfidence) const
{
	OutConfidence = 0.f;
	if (!GetPawn()) return nullptr;

	AActor* Best = nullptr;
	float BestScore = -FLT_MAX;
	for (const FPerceivedTarget& T : Memory)
	{
		AActor* Candidate = T.Actor.Get();
		if (!Candidate) continue;
		if (const ASoldierCharacter* S = Cast<ASoldierCharacter>(Candidate))
		{
			if (!S->IsAlive()) continue;
		}

		float Score = T.Confidence * 1000.f - FVector::DistSquared(T.LastKnownLocation, GetPawn()->GetActorLocation()) * 0.001f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
			OutConfidence = T.Confidence;
		}
	}
	return Best;
}

TArray<AActor*> ASoldierAIController::GetCurrentlyVisibleHostiles() const
{
	TArray<AActor*> Result;
	for (const FPerceivedTarget& T : Memory) if (T.bCurrentlyVisible && T.Actor.IsValid()) Result.Add(T.Actor.Get());
	return Result;
}

void ASoldierAIController::AddSuppression(float Amount) { Suppression = FMath::Clamp(Suppression + Amount, 0.f, 1.f); }
void ASoldierAIController::RequestCover(FVector TL, float R) {
	bHasPendingCover = false;
	bCoverQueryFinished = false;
	bLastCoverQuerySucceeded = false;
	LastCoverRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastCoverRequestTime;
	UCoverSystemSubsystem* CS = GetWorld()->GetSubsystem<UCoverSystemSubsystem>();
	if (!CS || !GetPawn()) {
		bCoverQueryFinished = true;
		bLastCoverQuerySucceeded = false;
		return;
	}
	TWeakObjectPtr<ASoldierAIController> WeakSelf(this);
	CS->RequestCoverAsync(GetPawn(), GetPawn()->GetActorLocation(), TL, R, [WeakSelf](bool bFound, const FCoverPoint& Res) {
		if (WeakSelf.IsValid()) {
			WeakSelf->PendingCoverResult = Res;
			WeakSelf->bHasPendingCover = bFound;
			WeakSelf->bCoverQueryFinished = true;
			WeakSelf->bLastCoverQuerySucceeded = bFound;
		}
	});
}

ASoldierCharacter* ASoldierAIController::GetSoldierCharacter() const { return Cast<ASoldierCharacter>(GetPawn()); }

bool ASoldierAIController::HasNearbyHostile(float Range) const
{
	if (!GetPawn()) return false;

	if (const USoldierRegistrySubsystem* Registry = GetWorld()->GetSubsystem<USoldierRegistrySubsystem>())
	{
		TArray<ASoldierCharacter*> Nearby = Registry->QueryRadius(GetPawn()->GetActorLocation(), Range);
		for (ASoldierCharacter* Other : Nearby)
		{
			if (!Other || Other == GetPawn() || !Other->IsAlive()) continue;
			if (Other->GetGenericTeamId().GetId() == GetGenericTeamId().GetId()) continue;
			return true;
		}
	}

	return false;
}

bool ASoldierAIController::HasCombatThreat(float MinConfidence) const
{
	if (!GetPawn()) return false;

	float Confidence = 0.f;
	AActor* Threat = GetPrimaryThreat(Confidence);
	if (Threat && Confidence >= MinConfidence)
	{
		return true;
	}

	// Fallback broad-phase using the soldier registry. This makes mission/follow tasks
	// stop even if the StateTree evaluator transition has not fired yet this frame.
	if (const USoldierRegistrySubsystem* Registry = GetWorld()->GetSubsystem<USoldierRegistrySubsystem>())
	{
		TArray<ASoldierCharacter*> Nearby = Registry->QueryRadius(GetPawn()->GetActorLocation(), DirectCombatRange);
		for (ASoldierCharacter* Other : Nearby)
		{
			if (!Other || Other == GetPawn() || !Other->IsAlive()) continue;
			if (Other->GetGenericTeamId().GetId() == GetGenericTeamId().GetId()) continue;

			const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Other->GetActorLocation());
			if (Dist <= DirectCombatCloseRangeOverride || HasLineOfSightTo(Other))
			{
				return true;
			}
		}
	}

	return false;
}

bool ASoldierAIController::HasLineOfSightTo(AActor* Target) const {
	if (!Target || !GetPawn()) return false;
	FHitResult Hit; FCollisionQueryParams P; P.AddIgnoredActor(GetPawn());
	const FVector Start = GetPawn()->GetActorLocation() + FVector(0,0,60);
	const FVector End = Target->GetActorLocation() + FVector(0,0,60);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, P)) return true;
	return Hit.GetActor() == Target;
}
