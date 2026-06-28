// SoldierCharacter.h — Base character for all AI soldiers
#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacter.h"
#include "GenericTeamAgentInterface.h"
#include "SquadTypes.h"
#include "SoldierCharacter.generated.h"

class UAimComponent;
class UWeaponComponent;
class UHealthComponent;
class UAIPerceptionStimuliSourceComponent;
class UAnimMontage;
class UMeshComponent;

UENUM(BlueprintType)
enum class ESoldierCoverSide : uint8
{
	Right UMETA(DisplayName = "Right"),
	Left  UMETA(DisplayName = "Left")
};

UENUM(BlueprintType)
enum class ESoldierCoverPeekType : uint8
{
	Side UMETA(DisplayName = "Side"),
	Over UMETA(DisplayName = "Over")
};

USTRUCT(BlueprintType)
struct FWeaponIKRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK")
	float Alpha = 0.f;

	// Component-space target for hand_l TwoBoneIK effector.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK")
	FVector LeftHandEffectorLocationCS = FVector::ZeroVector;

	// Component-space target rotation for optional hand_l Transform Modify Bone.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK")
	FRotator LeftHandEffectorRotationCS = FRotator::ZeroRotator;

	// Bone-space joint target for TwoBoneIK, relative to upperarm_l. Kept for simple/static setups.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK")
	FVector LeftElbowJointTargetBS = FVector(0.f, -80.f, 0.f);

	// Component-space dynamic joint target computed from the current animated lowerarm pose.
	// Prefer this for stable elbows across crouch/cover/aim animations.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK")
	FVector LeftElbowJointTargetCS = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK")
	bool bUseDynamicElbowTarget = false;

	// Optional socket-driven hand rotation, usually used with Transform Modify Bone in Component Space.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK")
	bool bUseLeftHandRotation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK")
	float LeftHandRotationAlpha = 0.f;

	// Safer additive rotation distribution. Use these with Transform Modify Bone nodes set
	// to Add To Existing / Bone Space. This prevents all twist being concentrated at the wrist.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK|Rotation Distribution")
	bool bUseLeftForearmAdditiveRotation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK|Rotation Distribution")
	FRotator LeftForearmAdditiveRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK|Rotation Distribution")
	float LeftForearmAdditiveRotationAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK|Rotation Distribution")
	bool bUseLeftHandAdditiveRotation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK|Rotation Distribution")
	FRotator LeftHandAdditiveRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK|Rotation Distribution")
	float LeftHandAdditiveRotationAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK|Grip")
	bool bUseWeaponGripPose = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK|Grip")
	float LeftHandGripAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon IK|Grip")
	float RightHandGripAlpha = 0.f;
};

USTRUCT(BlueprintType)
struct FSoldierAnimRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim")
	float Speed = 0.f;

	// Unreal-style locomotion direction in degrees: 0 forward, 90 right, -90 left, +/-180 back.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim")
	float Direction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim")
	bool bIsAlive = true;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim")
	bool bIsCrouched = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim")
	bool bCombatReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim")
	bool bWantsToFire = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Cover")
	bool bIsInCover = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Cover")
	bool bIsCrouchingInCover = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Cover")
	bool bIsPeekingFromCover = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Cover")
	bool bCoverIsLow = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Cover")
	bool bCoverSideIsLeft = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Cover")
	bool bCoverPeekIsOver = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Cover")
	bool bIsMovingInCover = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Cover")
	bool bIsAdjustingCover = false;

	// -1 = left along actor/cover right-vector, +1 = right, 0 = forward/back adjustment.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Cover")
	float CoverMoveDirection = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Aim")
	float AimYaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Aim")
	float AimPitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier Anim|Weapon IK")
	FWeaponIKRuntimeData WeaponIK;
};

UCLASS(Abstract, BlueprintType, Blueprintable)
class SQUADAI_API ASoldierCharacter : public ALyraCharacter
{
	GENERATED_BODY()

public:
	ASoldierCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soldier|Components")
	TObjectPtr<UAimComponent> AimComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soldier|Components")
	TObjectPtr<UWeaponComponent> WeaponComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soldier|Components")
	TObjectPtr<UHealthComponent> HealthComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soldier|Components")
	TObjectPtr<UAIPerceptionStimuliSourceComponent> PerceptionStimuli;

	// Optional visual/equipment mesh used by AnimBP IK. Assign this in Blueprint to the
	// rifle/gun mesh component attached to the character hand. It can be a StaticMeshComponent
	// or SkeletalMeshComponent because both derive from UMeshComponent.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon Visual")
	TObjectPtr<UMeshComponent> EquippedWeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon Visual")
	FName WeaponAttachSocketName = FName("RifleSocket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon Visual")
	FName WeaponLeftHandIKSocketName = FName("LeftHandIK");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon Visual")
	FName WeaponMuzzleSocketName = FName("Muzzle");

	// Optional automatic attachment tuning. If EquippedWeaponMesh is assigned and
	// bAutoAttachEquippedWeaponToMesh is true, SetEquippedWeaponMesh will attach the
	// weapon to this character Mesh at WeaponAttachSocketName and apply these offsets.
	// Default is false because many characters already have the weapon component attached
	// and scaled correctly in Blueprint. Enabling this will snap the weapon to the socket
	// at BeginPlay, so use only when you want C++ to manage attachment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon Visual|Attach")
	bool bAutoAttachEquippedWeaponToMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon Visual|Attach")
	FVector WeaponAttachLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon Visual|Attach")
	FRotator WeaponAttachRotationOffset = FRotator::ZeroRotator;

	// Keep false unless you intentionally want to override the weapon component scale.
	// Some imported FBX weapons rely on component scale, and forcing 1,1,1 can make them huge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon Visual|Attach")
	bool bOverrideWeaponAttachScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon Visual|Attach", meta = (EditCondition = "bOverrideWeaponAttachScale"))
	FVector WeaponAttachScale = FVector(1.f, 1.f, 1.f);

	// Centralized weapon IK tuning. Adjust these on BP_Leader/BP_Enemy once, instead
	// of rebuilding AnimBP math for every weapon/character.
	// Disabled by default now: the project is moving back to an AnimBP/socket-preview driven
	// weapon IK workflow. Enable only if an AnimBP explicitly consumes GetWeaponIKRuntimeData().
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK")
	bool bEnableLeftHandWeaponIK = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK", meta = (ClampMin = "0", ClampMax = "1"))
	float LeftHandWeaponIKAlpha = 1.f;

	// Extra component-space offset added after reading the weapon's LeftHandIK socket.
	// Use this for small hand placement corrections without moving the weapon socket.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK")
	FVector LeftHandIKEffectorLocationOffset_CS = FVector::ZeroVector;

	// Static fallback used with TwoBoneIK Joint Target Space = Bone Space,
	// Joint Target Bone = upperarm_l. Keep for testing/fallback.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Elbow")
	FVector LeftElbowJointTarget_BoneSpace = FVector(0.f, -80.f, 0.f);

	// Recommended: compute a dynamic elbow pole target from the current animated lowerarm
	// pose. This prevents elbow flips when crouching/peeking/transitioning.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Elbow")
	bool bUseDynamicLeftElbowTarget = true;

	// Bone/socket used as the moving base for the elbow pole target. Usually lowerarm_l.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Elbow")
	FName LeftElbowReferenceBoneName = FName("lowerarm_l");

	// If true, the offset is rotated by LeftElbowReferenceBoneName's current animated
	// component-space transform. This is usually more stable than a fixed component offset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Elbow")
	bool bLeftElbowOffsetInReferenceBoneSpace = true;

	// Offset from LeftElbowReferenceBoneName to where the IK pole target should be.
	// Tune this in BP_Leader/BP_Enemy. Start with (0,-60,0), then try sign/axis changes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Elbow")
	FVector LeftElbowDynamicOffset = FVector(0.f, -60.f, 0.f);

	// Optional socket-driven hand rotation. Keep disabled until elbow/location are correct.
	// If enabled, AnimBP can use LeftHandEffectorRotationCS with Transform Modify Bone on hand_l.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Socket Rotation")
	bool bUseLeftHandIKRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Socket Rotation", meta = (ClampMin = "0", ClampMax = "1"))
	float LeftHandIKRotationAlpha = 1.f;

	// Optional extra rotation offset added to the weapon socket rotation before driving hand_l.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Socket Rotation")
	FRotator LeftHandIKRotationOffset = FRotator::ZeroRotator;

	// Safer rotation correction path: distribute wrist/forearm twist across forearm + hand
	// using Additive/BoneSpace Transform Modify Bone nodes. Recommended before trying
	// full socket-driven Replace rotation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Rotation Distribution")
	bool bUseLeftForearmAdditiveRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Rotation Distribution")
	FRotator LeftForearmAdditiveRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Rotation Distribution", meta = (ClampMin = "0", ClampMax = "1"))
	float LeftForearmAdditiveRotationAlpha = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Rotation Distribution")
	bool bUseLeftHandAdditiveRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Rotation Distribution")
	FRotator LeftHandAdditiveRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Rotation Distribution", meta = (ClampMin = "0", ClampMax = "1"))
	float LeftHandAdditiveRotationAlpha = 0.65f;

	// Finger/grip pose blending. AnimBP should use these alphas to layer a static rifle
	// grip pose onto hand_l/hand_r and their finger children. This avoids retargeting
	// fragile finger chains while still giving a proper weapon grip.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Grip")
	bool bEnableWeaponGripPose = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Grip", meta = (ClampMin = "0", ClampMax = "1"))
	float LeftHandGripAlpha = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Weapon IK|Grip", meta = (ClampMin = "0", ClampMax = "1"))
	float RightHandGripAlpha = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Faction")
	ESquadFaction Faction = ESquadFaction::Enemy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Faction")
	float AccuracyMultiplier = 0.65f;

	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& InTeamId) override { TeamId = InTeamId; }

	UPROPERTY(BlueprintReadOnly, Category = "Soldier|Cover")
	bool bIsInCover = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier|Cover")
	bool bIsCrouchingInCover = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier|Cover")
	FCoverPoint CurrentCoverPoint;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier|Cover")
	ESoldierCoverSide CoverSide = ESoldierCoverSide::Right;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier|Cover")
	ESoldierCoverPeekType CoverPeekType = ESoldierCoverPeekType::Side;

	UFUNCTION(BlueprintCallable, Category = "Soldier|Cover")
	void SetCoverState(bool bInCover, bool bCrouching);

	UFUNCTION(BlueprintCallable, Category = "Soldier|Cover")
	void SetCurrentCover(const FCoverPoint& Cover);

	UFUNCTION(BlueprintCallable, Category = "Soldier|Cover")
	void ClearCover();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier")
	bool IsAlive() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier")
	bool IsEnemy() const { return Faction == ESquadFaction::Enemy; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier")
	bool IsAlly() const { return Faction == ESquadFaction::Ally; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier")
	float GetHealthPercent() const;

	// AnimBP helpers. These prevent the AnimBP from having to duplicate movement logic.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Animation")
	float GetGroundSpeed() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Animation")
	bool IsMovingInCover(float SpeedThreshold = 20.f) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Cover")
	bool IsCurrentCoverLow() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Cover")
	bool IsCoverPeekOver() const { return CoverPeekType == ESoldierCoverPeekType::Over; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Animation")
	UAnimMontage* SelectFireMontage() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Animation")
	UAnimMontage* SelectReloadMontage() const;

	UFUNCTION(BlueprintCallable, Category = "Soldier|Weapon Visual")
	void SetEquippedWeaponMesh(UMeshComponent* InWeaponMesh);

	UFUNCTION(BlueprintCallable, Category = "Soldier|Weapon Visual")
	void ApplyEquippedWeaponAttachment();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Weapon Visual")
	bool GetEquippedWeaponSocketTransform(FName SocketName, FTransform& OutTransform) const;

	// Same as above, but converted into this character mesh component space. This is
	// the easiest form to feed into AnimGraph IK nodes such as FABRIK / TwoBoneIK.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Weapon Visual")
	bool GetEquippedWeaponSocketTransformInMeshSpace(FName SocketName, FTransform& OutTransform) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Weapon Visual")
	bool GetLeftHandIKWorldTransform(FTransform& OutTransform) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Weapon Visual")
	bool GetLeftHandIKMeshSpaceTransform(FTransform& OutTransform) const;

	// New helper: Gets the LeftHandIK socket transform relative to the hand_r bone. 
	// Directly plug this into the FABRIK node Effector Transform (when set to Bone Space).
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Weapon Visual")
	bool GetLeftHandIKTransformRelativeToRightHand(FTransform& OutTransform) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Weapon Visual")
	bool GetMuzzleWorldTransform(FTransform& OutTransform) const;

	// One-stop AnimBP helper for left-hand weapon IK. Use this instead of duplicating
	// transform conversion math in every AnimBP.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Weapon IK")
	FWeaponIKRuntimeData GetWeaponIKRuntimeData() const;

	// One-stop AnimBP helper for almost every variable the soldier AnimBP needs.
	// Prefer this over duplicating speed/direction/cover/aim/IK math in Blueprint.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Soldier|Animation")
	FSoldierAnimRuntimeData GetSoldierAnimRuntimeData(float CoverMoveSpeedThreshold = 20.f, float CoverAdjustDirectionThreshold = 0.35f, float CombatThreatConfidence = 0.25f) const;

	UFUNCTION(BlueprintCallable, Category = "Soldier|Animation")
	void PlayFireAnimation();

	UFUNCTION(BlueprintCallable, Category = "Soldier|Animation")
	void PlayReloadAnimation();

	// ---- GLOBAL GOAL (Automation) ----
	UPROPERTY(BlueprintReadWrite, Category = "Soldier|Mission")
	FVector GlobalGoalLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Soldier|Mission")
	bool bHasGlobalGoal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	// Optional firing montage. Assign this in BP_EnemySoldier/BP_AllySoldier/BP_Leader.
	// If left empty, gameplay shooting still works; only the visual firing animation is missing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Fire")
	TObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Fire")
	TObjectPtr<UAnimMontage> FireMontage_Crouch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Fire")
	TObjectPtr<UAnimMontage> FireMontage_CoverHighRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Fire")
	TObjectPtr<UAnimMontage> FireMontage_CoverHighLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Fire")
	TObjectPtr<UAnimMontage> FireMontage_CoverLowRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Fire")
	TObjectPtr<UAnimMontage> FireMontage_CoverLowLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Fire")
	TObjectPtr<UAnimMontage> FireMontage_CoverLowOver;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Reload")
	TObjectPtr<UAnimMontage> ReloadMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Reload")
	TObjectPtr<UAnimMontage> ReloadMontage_Crouch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Reload")
	TObjectPtr<UAnimMontage> ReloadMontage_CoverHighRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Reload")
	TObjectPtr<UAnimMontage> ReloadMontage_CoverHighLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Reload")
	TObjectPtr<UAnimMontage> ReloadMontage_CoverLowRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Reload")
	TObjectPtr<UAnimMontage> ReloadMontage_CoverLowLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation|Reload")
	TObjectPtr<UAnimMontage> ReloadMontage_CoverLowOver;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier|Animation")
	bool bWantsToFire = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soldier|Animation")
	float LastFireAnimTime = -1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation")
	TArray<TObjectPtr<UAnimMontage>> HitReactMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation")
	TArray<TObjectPtr<UAnimMontage>> DeathMontages;

	// If a DeathMontage plays successfully, ragdoll after the montage has had time to show.
	// This is a minimum delay; code will also consider the montage length.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soldier|Animation")
	float DeathMontageToRagdollDelay = 2.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	// AUTO-LYRA SETUP: Ensure Lyra injects cosmetic parts (like the mesh/weapons) immediately when possessed
	virtual void PossessedBy(AController* NewController) override;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION()
	void OnHealthDied(AActor* OwnerActor);
	
	UFUNCTION()
	void OnLyraHealthDied(AActor* OwningActor);

	virtual void HandleDeath();

	void RegisterWithSystems();
	void UnregisterFromSystems();

	FGenericTeamId TeamId;
};
