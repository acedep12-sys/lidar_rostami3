// SoldierCharacter.cpp — Base soldier with components, faction, cover state
#include "Characters/SoldierCharacter.h"
#include "AI/SoldierAIController.h"
#include "Components/AimComponent.h"
#include "Components/WeaponComponent.h"
#include "Components/HealthComponent.h"
#include "Performance/AISignificanceManager.h"
#include "Performance/SoldierRegistrySubsystem.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Damage.h"
#include "Components/CapsuleComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/DamageEvents.h"
#include "TimerManager.h"
#include "SquadAITuning.h"
#include "Engine/World.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Character/LyraPawnData.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraHealthComponent.h"

ASoldierCharacter::ASoldierCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// AUTO-LYRA SETUP: Make sure AI Controller is assigned by default
	AIControllerClass = ASoldierAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	AimComp = CreateDefaultSubobject<UAimComponent>(TEXT("AimComp"));
	WeaponComp = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComp"));
	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComp"));

	bUseControllerRotationYaw = false; 
	if (UCharacterMovementComponent* CMC = GetCharacterMovement()) {
		CMC->bOrientRotationToMovement = true; 
		CMC->bUseControllerDesiredRotation = true;
		CMC->RotationRate = FRotator(0.f, 720.f, 0.f); // Very fast for tight corners
		CMC->bRequestedMoveUseAcceleration = true; 
		CMC->MaxWalkSpeed = 400.f;
		CMC->MaxStepHeight = 40.f;
		// Required for cover/engage tasks that crouch behind low cover.
		CMC->GetNavAgentPropertiesRef().bCanCrouch = true;
		CMC->SetCrouchedHalfHeight(44.f);
	}

	PerceptionStimuli = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("PerceptionStimuli"));
	PerceptionStimuli->bAutoRegister = true;
	PerceptionStimuli->RegisterForSense(UAISense_Sight::StaticClass());
	PerceptionStimuli->RegisterForSense(UAISense_Hearing::StaticClass());
	PerceptionStimuli->RegisterForSense(UAISense_Damage::StaticClass());

	// Safe default for placed pawns before BeginPlay runs.
	TeamId = FGenericTeamId(1);
}

void ASoldierCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// AUTO-LYRA SETUP: Force the PawnExtension component to initialize immediately when the AI takes control.
	// This fixes the "Invisible Floating Guns" bug by pushing Lyra's character parts into the mesh.
	if (ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}
}

void ASoldierCharacter::BeginPlay() {
	// AUTO-LYRA SETUP: If no PawnData is assigned (e.g. dragged into map manually), assign the default!
	if (ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		if (!PawnExtComp->GetPawnData<ULyraPawnData>())
		{
			if (ULyraPawnData* DefaultData = LoadObject<ULyraPawnData>(nullptr, TEXT("/ShooterCore/Game/HeroData_ShooterGame.HeroData_ShooterGame")))
			{
				PawnExtComp->SetPawnData(DefaultData);
			}
		}
	}

	Super::BeginPlay();
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		// Runtime enforcement for existing Blueprints that may have serialized bCanCrouch=false.
		CMC->GetNavAgentPropertiesRef().bCanCrouch = true;
		CMC->SetCrouchedHalfHeight(44.f);
	}
	TeamId = (Faction == ESquadFaction::Enemy) ? FGenericTeamId(1) : FGenericTeamId(0);

	// Possession may have happened before BeginPlay. Keep the controller's team in sync
	// so perception/attitude checks do not see enemies as NoTeam/neutral.
	if (ASoldierAIController* AIC = Cast<ASoldierAIController>(GetController()))
	{
		AIC->SetGenericTeamId(TeamId);
	}

	RegisterWithSystems();
	if (WeaponComp) WeaponComp->AccuracyMultiplier = AccuracyMultiplier;
	if (HealthComp) HealthComp->OnDeath.AddDynamic(this, &ASoldierCharacter::OnHealthDied);
	
	// AUTO-LYRA SETUP: Bind to Lyra's Native Health Death Event as well
	if (ULyraHealthComponent* LyraHealth = ULyraHealthComponent::FindHealthComponent(this))
	{
		LyraHealth->OnDeathStarted.AddDynamic(this, &ASoldierCharacter::OnLyraHealthDied);
	}
}

void ASoldierCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason) { UnregisterFromSystems(); Super::EndPlay(EndPlayReason); }

float ASoldierCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) {
	if (HealthComp && HealthComp->IsAlive()) {
		HealthComp->ApplyDamage(DamageAmount, DamageCauser);
		if (HealthComp->IsAlive()) {
			UAnimMontage* ReactMontage = HitReactMontage;
			if (HitReactMontages.Num() > 0)
			{
				ReactMontage = HitReactMontages[FMath::RandRange(0, HitReactMontages.Num() - 1)];
			}
			if (ReactMontage)
			{
				if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
					if (!AnimInst->Montage_IsPlaying(ReactMontage)) AnimInst->Montage_Play(ReactMontage, 1.f);
			}
		}
		UAISense_Damage::ReportDamageEvent(GetWorld(), this, DamageCauser, DamageAmount, GetActorLocation(), DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation());
	}
	return DamageAmount;
}

void ASoldierCharacter::OnHealthDied(AActor* OwnerActor) { HandleDeath(); }

void ASoldierCharacter::OnLyraHealthDied(AActor* OwningActor) { HandleDeath(); }

void ASoldierCharacter::HandleDeath() {
	UnregisterFromSystems();
	if (WeaponComp) WeaponComp->StopFiring(); if (AimComp) AimComp->ClearAimTarget();

	if (ASoldierAIController* AIC = Cast<ASoldierAIController>(GetController()))
	{
		AIC->StopMovement();
		if (AIC->StateTreeComp)
		{
			AIC->StateTreeComp->StopLogic(TEXT("Dead"));
		}
	}

	if (GetCharacterMovement()) { GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->DisableMovement(); }
	if (UCapsuleComponent* Capsule = GetCapsuleComponent()) { Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); }

	float PlayedLength = 0.f;
	UAnimMontage* SelectedDeathMontage = DeathMontage;
	if (DeathMontages.Num() > 0)
	{
		SelectedDeathMontage = DeathMontages[FMath::RandRange(0, DeathMontages.Num() - 1)];
	}

	if (SelectedDeathMontage && GetMesh())
	{
		if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
		{
			PlayedLength = AnimInst->Montage_Play(SelectedDeathMontage, 1.f);
			UE_LOG(LogTemp, Warning, TEXT("DeathAnim: %s Montage_Play(%s) on AnimBP=%s returned %.3f"),
				*GetNameSafe(this), *GetNameSafe(SelectedDeathMontage), *GetNameSafe(AnimInst->GetClass()), PlayedLength);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("DeathAnim: %s has DeathMontage %s but mesh has no AnimInstance."),
				*GetNameSafe(this), *GetNameSafe(SelectedDeathMontage));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DeathAnim: %s has no DeathMontage assigned; using ragdoll fallback."), *GetNameSafe(this));
	}

	if (GetMesh())
	{
		if (PlayedLength > 0.f)
		{
			// Let the death animation play first, then optionally ragdoll. This prevents the
			// ragdoll from instantly hiding the montage.
			const float RagdollDelay = FMath::Max(DeathMontageToRagdollDelay, PlayedLength * 0.90f);
			TWeakObjectPtr<ASoldierCharacter> WeakSelf(this);
			FTimerHandle RagdollTimer;
			GetWorldTimerManager().SetTimer(RagdollTimer, [WeakSelf]()
			{
				if (!WeakSelf.IsValid() || !WeakSelf->GetMesh()) return;
				WeakSelf->GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
				WeakSelf->GetMesh()->SetSimulatePhysics(true);
			}, RagdollDelay, false);
		}
		else
		{
			// Fallback visual death if no montage is assigned or montage failed to start.
			GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
			GetMesh()->SetSimulatePhysics(true);
		}
	}

	DetachFromControllerPendingDestroy();
	SetLifeSpan(10.f);
}

void ASoldierCharacter::RegisterWithSystems() {
	if (UWorld* World = GetWorld()) {
		if (UAISignificanceManager* SM = World->GetSubsystem<UAISignificanceManager>()) SM->RegisterSoldier(this);
		if (USoldierRegistrySubsystem* Reg = World->GetSubsystem<USoldierRegistrySubsystem>()) Reg->RegisterSoldier(this);
	}
}

void ASoldierCharacter::UnregisterFromSystems() {
	if (UWorld* World = GetWorld()) {
		if (UAISignificanceManager* SM = World->GetSubsystem<UAISignificanceManager>()) SM->UnregisterSoldier(this);
		if (USoldierRegistrySubsystem* Reg = World->GetSubsystem<USoldierRegistrySubsystem>()) Reg->UnregisterSoldier(this);
	}
}

void ASoldierCharacter::SetCoverState(bool bInCover, bool bCrouching) {
	bIsInCover = bInCover; bIsCrouchingInCover = bCrouching;
	if (bCrouching && !bIsCrouched) Crouch(); else if (!bCrouching && bIsCrouched) UnCrouch();
}

void ASoldierCharacter::SetCurrentCover(const FCoverPoint& Cover)
{
	// This only stores the selected/reserved cover point.
	// Do NOT mark the soldier as actually being in cover here: this is called as soon
	// as a cover point is found, while the pawn may still be far away and moving to it.
	// bIsInCover must become true only after arrival via SetCoverState(). Otherwise
	// the AnimBP enters CoverLow/CoverHigh idle too early and the character slides.
	CurrentCoverPoint = Cover;

	// Pick a default cover side/peek type from the scanner's available lean data.
	// AnimBP can use these for CoverHi/CoverLo left/right/over animations.
	if (EnumHasAnyFlags(Cover.AvailableLeans, ELeanSide::Right))
	{
		CoverSide = ESoldierCoverSide::Right;
	}
	else if (EnumHasAnyFlags(Cover.AvailableLeans, ELeanSide::Left))
	{
		CoverSide = ESoldierCoverSide::Left;
	}
	else
	{
		CoverSide = ESoldierCoverSide::Right;
	}

	if (Cover.Height == ECoverHeight::Low && EnumHasAnyFlags(Cover.AvailableLeans, ELeanSide::Over))
	{
		CoverPeekType = ESoldierCoverPeekType::Over;
	}
	else
	{
		CoverPeekType = ESoldierCoverPeekType::Side;
	}
}
void ASoldierCharacter::ClearCover() { bIsInCover = false; if (bIsCrouched) UnCrouch(); }
bool ASoldierCharacter::IsAlive() const { return HealthComp ? HealthComp->IsAlive() : true; }
float ASoldierCharacter::GetHealthPercent() const { return HealthComp ? HealthComp->GetHealthPercent() : 1.f; }

float ASoldierCharacter::GetGroundSpeed() const
{
	return GetVelocity().Size2D();
}

bool ASoldierCharacter::IsMovingInCover(float SpeedThreshold) const
{
	return bIsInCover && GetGroundSpeed() > SpeedThreshold;
}

bool ASoldierCharacter::IsCurrentCoverLow() const
{
	return CurrentCoverPoint.Height == ECoverHeight::Low;
}

UAnimMontage* ASoldierCharacter::SelectFireMontage() const
{
	if (bIsInCover)
	{
		if (IsCurrentCoverLow())
		{
			if (CoverPeekType == ESoldierCoverPeekType::Over && FireMontage_CoverLowOver)
			{
				return FireMontage_CoverLowOver;
			}
			if (CoverSide == ESoldierCoverSide::Left && FireMontage_CoverLowLeft)
			{
				return FireMontage_CoverLowLeft;
			}
			if (FireMontage_CoverLowRight)
			{
				return FireMontage_CoverLowRight;
			}
		}
		else
		{
			if (CoverSide == ESoldierCoverSide::Left && FireMontage_CoverHighLeft)
			{
				return FireMontage_CoverHighLeft;
			}
			if (FireMontage_CoverHighRight)
			{
				return FireMontage_CoverHighRight;
			}
		}
	}

	if (bIsCrouched && FireMontage_Crouch)
	{
		return FireMontage_Crouch;
	}

	return FireMontage;
}

UAnimMontage* ASoldierCharacter::SelectReloadMontage() const
{
	if (bIsInCover)
	{
		if (IsCurrentCoverLow())
		{
			if (CoverPeekType == ESoldierCoverPeekType::Over && ReloadMontage_CoverLowOver)
			{
				return ReloadMontage_CoverLowOver;
			}
			if (CoverSide == ESoldierCoverSide::Left && ReloadMontage_CoverLowLeft)
			{
				return ReloadMontage_CoverLowLeft;
			}
			if (ReloadMontage_CoverLowRight)
			{
				return ReloadMontage_CoverLowRight;
			}
		}
		else
		{
			if (CoverSide == ESoldierCoverSide::Left && ReloadMontage_CoverHighLeft)
			{
				return ReloadMontage_CoverHighLeft;
			}
			if (ReloadMontage_CoverHighRight)
			{
				return ReloadMontage_CoverHighRight;
			}
		}
	}

	if (bIsCrouched && ReloadMontage_Crouch)
	{
		return ReloadMontage_Crouch;
	}

	return ReloadMontage;
}

void ASoldierCharacter::SetEquippedWeaponMesh(UMeshComponent* InWeaponMesh)
{
	EquippedWeaponMesh = InWeaponMesh;
	ApplyEquippedWeaponAttachment();
}

void ASoldierCharacter::ApplyEquippedWeaponAttachment()
{
	if (!EquippedWeaponMesh || !GetMesh() || !bAutoAttachEquippedWeaponToMesh)
	{
		return;
	}

	EquippedWeaponMesh->AttachToComponent(
		GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		WeaponAttachSocketName);

	EquippedWeaponMesh->SetRelativeLocation(WeaponAttachLocationOffset);
	EquippedWeaponMesh->SetRelativeRotation(WeaponAttachRotationOffset);
	if (bOverrideWeaponAttachScale)
	{
		EquippedWeaponMesh->SetRelativeScale3D(WeaponAttachScale);
	}
	EquippedWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ASoldierCharacter::GetEquippedWeaponSocketTransform(FName SocketName, FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	if (!EquippedWeaponMesh || SocketName.IsNone())
	{
		return false;
	}

	if (!EquippedWeaponMesh->DoesSocketExist(SocketName))
	{
		return false;
	}

	OutTransform = EquippedWeaponMesh->GetSocketTransform(SocketName, RTS_World);
	return true;
}

bool ASoldierCharacter::GetEquippedWeaponSocketTransformInMeshSpace(FName SocketName, FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;

	FTransform SocketWorld;
	if (!GetEquippedWeaponSocketTransform(SocketName, SocketWorld))
	{
		return false;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return false;
	}

	// Convert weapon socket world transform into the character mesh's component space.
	OutTransform = SocketWorld.GetRelativeTransform(MeshComp->GetComponentTransform());
	return true;
}

bool ASoldierCharacter::GetLeftHandIKWorldTransform(FTransform& OutTransform) const
{
	return GetEquippedWeaponSocketTransform(WeaponLeftHandIKSocketName, OutTransform);
}

bool ASoldierCharacter::GetLeftHandIKMeshSpaceTransform(FTransform& OutTransform) const
{
	return GetEquippedWeaponSocketTransformInMeshSpace(WeaponLeftHandIKSocketName, OutTransform);
}

bool ASoldierCharacter::GetLeftHandIKTransformRelativeToRightHand(FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	
	FTransform LeftHandWorld;
	if (!GetLeftHandIKWorldTransform(LeftHandWorld))
	{
		return false;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return false;
	}

	// Make the left hand socket transform relative to the character's right hand.
	// We use "hand_r" as hardcoded, because that's where the weapon is attached.
	FTransform RightHandWorld = MeshComp->GetSocketTransform(FName("hand_r"), RTS_World);
	
	// Convert World to Relative (Bone Space)
	OutTransform = LeftHandWorld.GetRelativeTransform(RightHandWorld);
	return true;
}

bool ASoldierCharacter::GetMuzzleWorldTransform(FTransform& OutTransform) const
{
	return GetEquippedWeaponSocketTransform(WeaponMuzzleSocketName, OutTransform);
}

FWeaponIKRuntimeData ASoldierCharacter::GetWeaponIKRuntimeData() const
{
	FWeaponIKRuntimeData Data;
	Data.bEnabled = false;
	Data.Alpha = 0.f;
	Data.LeftElbowJointTargetBS = LeftElbowJointTarget_BoneSpace;

	if (!bEnableLeftHandWeaponIK || !IsAlive())
	{
		return Data;
	}

	FTransform LeftHandMeshSpace;
	if (!GetLeftHandIKMeshSpaceTransform(LeftHandMeshSpace))
	{
		return Data;
	}

	Data.bEnabled = true;
	Data.Alpha = LeftHandWeaponIKAlpha;
	Data.LeftHandEffectorLocationCS = LeftHandMeshSpace.GetLocation() + LeftHandIKEffectorLocationOffset_CS;
	Data.LeftHandEffectorRotationCS = (LeftHandMeshSpace.GetRotation().Rotator() + LeftHandIKRotationOffset).GetNormalized();
	Data.LeftElbowJointTargetBS = LeftElbowJointTarget_BoneSpace;

	if (bUseDynamicLeftElbowTarget)
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			const bool bHasRefBone = MeshComp->GetBoneIndex(LeftElbowReferenceBoneName) != INDEX_NONE || MeshComp->DoesSocketExist(LeftElbowReferenceBoneName);
			if (bHasRefBone)
			{
				const FTransform RefCS = MeshComp->GetSocketTransform(LeftElbowReferenceBoneName, RTS_Component);
				Data.LeftElbowJointTargetCS = bLeftElbowOffsetInReferenceBoneSpace
					? RefCS.TransformPosition(LeftElbowDynamicOffset)
					: RefCS.GetLocation() + LeftElbowDynamicOffset;
				Data.bUseDynamicElbowTarget = true;
			}
		}
	}

	Data.bUseLeftHandRotation = bUseLeftHandIKRotation;
	Data.LeftHandRotationAlpha = bUseLeftHandIKRotation ? LeftHandIKRotationAlpha : 0.f;
	Data.bUseLeftForearmAdditiveRotation = bUseLeftForearmAdditiveRotation;
	Data.LeftForearmAdditiveRotation = LeftForearmAdditiveRotationOffset;
	Data.LeftForearmAdditiveRotationAlpha = bUseLeftForearmAdditiveRotation ? LeftForearmAdditiveRotationAlpha : 0.f;
	Data.bUseLeftHandAdditiveRotation = bUseLeftHandAdditiveRotation;
	Data.LeftHandAdditiveRotation = LeftHandAdditiveRotationOffset;
	Data.LeftHandAdditiveRotationAlpha = bUseLeftHandAdditiveRotation ? LeftHandAdditiveRotationAlpha : 0.f;
	Data.bUseWeaponGripPose = bEnableWeaponGripPose;
	Data.LeftHandGripAlpha = bEnableWeaponGripPose ? LeftHandGripAlpha : 0.f;
	Data.RightHandGripAlpha = bEnableWeaponGripPose ? RightHandGripAlpha : 0.f;
	return Data;
}

FSoldierAnimRuntimeData ASoldierCharacter::GetSoldierAnimRuntimeData(float CoverMoveSpeedThreshold, float CoverAdjustDirectionThreshold, float CombatThreatConfidence) const
{
	FSoldierAnimRuntimeData Data;

	const FVector Velocity = GetVelocity();
	const FVector Velocity2D(Velocity.X, Velocity.Y, 0.f);
	Data.Speed = Velocity2D.Size();
	Data.Direction = 0.f;
	if (Data.Speed > KINDA_SMALL_NUMBER)
	{
		const FVector MoveDir = Velocity2D.GetSafeNormal();
		const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
		const FVector Right = GetActorRightVector().GetSafeNormal2D();
		const float ForwardDot = FVector::DotProduct(MoveDir, Forward);
		const float RightDot = FVector::DotProduct(MoveDir, Right);
		Data.Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
	}

	Data.bIsAlive = IsAlive();
	Data.bIsCrouched = bIsCrouched;
	Data.bWantsToFire = bWantsToFire;
	Data.bIsInCover = bIsInCover;
	Data.bIsCrouchingInCover = bIsCrouchingInCover;
	Data.bIsPeekingFromCover = bIsInCover && !bIsCrouchingInCover;
	Data.bCoverIsLow = IsCurrentCoverLow();
	Data.bCoverSideIsLeft = CoverSide == ESoldierCoverSide::Left;
	Data.bCoverPeekIsOver = CoverPeekType == ESoldierCoverPeekType::Over;
	Data.bIsMovingInCover = bIsInCover && Data.Speed > CoverMoveSpeedThreshold;

	const FVector MoveDir = Data.Speed > KINDA_SMALL_NUMBER ? Velocity2D.GetSafeNormal() : FVector::ZeroVector;
	Data.CoverMoveDirection = Data.Speed > KINDA_SMALL_NUMBER ? FVector::DotProduct(MoveDir, GetActorRightVector().GetSafeNormal2D()) : 0.f;
	Data.bIsAdjustingCover = Data.bIsMovingInCover && FMath::Abs(Data.CoverMoveDirection) < CoverAdjustDirectionThreshold;

	if (AimComp)
	{
		const FRotator AimOffset = AimComp->GetSpineAimOffset();
		Data.AimYaw = FMath::Clamp(AimOffset.Yaw, -90.f, 90.f);
		Data.AimPitch = FMath::Clamp(AimOffset.Pitch, -90.f, 90.f);
	}

	if (const ASoldierAIController* AIC = Cast<ASoldierAIController>(GetController()))
	{
		Data.bCombatReady = AIC->HasCombatThreat(CombatThreatConfidence);
	}
	else
	{
		Data.bCombatReady = false;
	}

	Data.WeaponIK = GetWeaponIKRuntimeData();
	return Data;
}

void ASoldierCharacter::PlayFireAnimation()
{
	bWantsToFire = true;
	LastFireAnimTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	UAnimMontage* MontageToPlay = SelectFireMontage();
	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("FireAnim: %s wants to fire but no suitable FireMontage is assigned. InCover=%d Low=%d Side=%s Peek=%s Crouched=%d"),
			*GetNameSafe(this), bIsInCover ? 1 : 0, IsCurrentCoverLow() ? 1 : 0,
			CoverSide == ESoldierCoverSide::Left ? TEXT("Left") : TEXT("Right"),
			CoverPeekType == ESoldierCoverPeekType::Over ? TEXT("Over") : TEXT("Side"),
			bIsCrouched ? 1 : 0);
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("FireAnim: %s has montage %s but GetMesh() is null."), *GetNameSafe(this), *GetNameSafe(MontageToPlay));
		return;
	}

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst)
	{
		UE_LOG(LogTemp, Warning, TEXT("FireAnim: %s has montage %s but mesh %s has no AnimInstance. Check Anim Class on Mesh."),
			*GetNameSafe(this), *GetNameSafe(MontageToPlay), *GetNameSafe(MeshComp->GetSkeletalMeshAsset()));
		return;
	}

	// Fire montages are usually short upper-body montages. If designers use a full-body
	// montage, it will still play, but an upper-body slot is recommended.
	const float PlayedLength = AnimInst->Montage_Play(MontageToPlay, 1.f);
	UE_LOG(LogTemp, Warning, TEXT("FireAnim: %s Montage_Play(%s) on AnimBP=%s returned %.3f. Mesh=%s InCover=%d Low=%d Side=%s Peek=%s"),
		*GetNameSafe(this),
		*GetNameSafe(MontageToPlay),
		*GetNameSafe(AnimInst->GetClass()),
		PlayedLength,
		*GetNameSafe(MeshComp->GetSkeletalMeshAsset()),
		bIsInCover ? 1 : 0,
		IsCurrentCoverLow() ? 1 : 0,
		CoverSide == ESoldierCoverSide::Left ? TEXT("Left") : TEXT("Right"),
		CoverPeekType == ESoldierCoverPeekType::Over ? TEXT("Over") : TEXT("Side"));

	if (PlayedLength <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("FireAnim: Montage did not start. Usually skeleton mismatch, bad slot setup, or montage incompatible with this AnimInstance."));
	}
}

void ASoldierCharacter::PlayReloadAnimation()
{
	UAnimMontage* MontageToPlay = SelectReloadMontage();
	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Verbose, TEXT("ReloadAnim: %s requested reload but no suitable ReloadMontage is assigned."), *GetNameSafe(this));
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInst)
	{
		UE_LOG(LogTemp, Warning, TEXT("ReloadAnim: %s has montage %s but no AnimInstance."), *GetNameSafe(this), *GetNameSafe(MontageToPlay));
		return;
	}

	const float PlayedLength = AnimInst->Montage_Play(MontageToPlay, 1.f);
	UE_LOG(LogTemp, Warning, TEXT("ReloadAnim: %s Montage_Play(%s) returned %.3f InCover=%d Low=%d Side=%s Peek=%s"),
		*GetNameSafe(this), *GetNameSafe(MontageToPlay), PlayedLength,
		bIsInCover ? 1 : 0,
		IsCurrentCoverLow() ? 1 : 0,
		CoverSide == ESoldierCoverSide::Left ? TEXT("Left") : TEXT("Right"),
		CoverPeekType == ESoldierCoverPeekType::Over ? TEXT("Over") : TEXT("Side"));
}
