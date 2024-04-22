// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/RPlayerBase.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Player/RPlayerController.h"

#include "GameplayAbilities/RAbilitySystemComponent.h"
#include "GameplayAbilities/RAttributeSet.h"
#include "GameplayAbilities/RAbilityGenericTags.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayAbilities/RAbilityGenericTags.h"
#include "Animation/AnimInstance.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"

#include "Net/UnrealNetwork.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"

ARPlayerBase::ARPlayerBase()
{
	viewPivot = CreateDefaultSubobject<USceneComponent>("Camera Pivot");
	viewCamera = CreateDefaultSubobject<UCameraComponent>("View Camera");

	viewCamera->SetupAttachment(viewPivot, USpringArmComponent::SocketName);
	viewCamera->bUsePawnControlRotation = false;
	viewCamera->SetRelativeLocation(DefaultCameraLocal);

	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(1080.f);
	GetCharacterMovement()->JumpZVelocity = 600.f;

	cameraClampMax = 10;
	cameraClampMin = -60;
	bIsScoping = false;
}

void ARPlayerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	viewPivot->SetRelativeLocation(GetActorLocation()); // centers the pivot on the player without getting the players rotation
	
	if (bIsScoping)
	{
		FRotator playerRot = viewPivot->GetRelativeRotation();
		playerRot.Roll = 0;
		SetActorRotation(playerRot);
	}
}

void ARPlayerBase::BeginPlay()
{
	Super::BeginPlay();

	playerController = Cast<ARPlayerController>(GetController());
}

void ARPlayerBase::PawnClientRestart()
{
	Super::PawnClientRestart();

	APlayerController* PlayerController = GetController<APlayerController>();
	if (PlayerController)
	{
		UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
		InputSubsystem->ClearAllMappings();
		InputSubsystem->AddMappingContext(inputMapping, 0);
	}
}

void ARPlayerBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* enhancedInputComp = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (enhancedInputComp)
	{
		enhancedInputComp->BindAction(moveInputAction, ETriggerEvent::Triggered, this, &ARPlayerBase::Move);
		enhancedInputComp->BindAction(lookInputAction, ETriggerEvent::Triggered, this, &ARPlayerBase::Look);
		enhancedInputComp->BindAction(jumpInputAction, ETriggerEvent::Triggered, this, &ARPlayerBase::Jump);
		enhancedInputComp->BindAction(QuitOutAction, ETriggerEvent::Triggered, this, &ARPlayerBase::QuitOut);
		enhancedInputComp->BindAction(basicAttackAction, ETriggerEvent::Triggered, this, &ARPlayerBase::DoBasicAttack);
		enhancedInputComp->BindAction(scopeInputAction, ETriggerEvent::Started, this, &ARPlayerBase::EnableScoping);
		enhancedInputComp->BindAction(scopeInputAction, ETriggerEvent::Triggered, this, &ARPlayerBase::DisableScoping);
		enhancedInputComp->BindAction(specialAttackAction, ETriggerEvent::Triggered, this, &ARPlayerBase::TryActivateSpecialAttack);
		enhancedInputComp->BindAction(ultimateAttackAction, ETriggerEvent::Triggered, this, &ARPlayerBase::TryActivateUltimateAttack);
		enhancedInputComp->BindAction(AbilityConfirmAction, ETriggerEvent::Triggered, this, &ARPlayerBase::ConfirmActionTriggered);
		enhancedInputComp->BindAction(AbilityCancelAction, ETriggerEvent::Triggered, this, &ARPlayerBase::CancelActionTriggered);
	}
}

void ARPlayerBase::Move(const FInputActionValue& InputValue)
{
	FVector2D input = InputValue.Get<FVector2D>();
	input.Normalize();

	AddMovementInput(input.Y * GetMoveFwdDir() + input.X * GetMoveRightDir());
}

void ARPlayerBase::Look(const FInputActionValue& InputValue)
{
	FVector2D input = InputValue.Get<FVector2D>();

	FRotator newRot = viewPivot->GetComponentRotation();
	newRot.Pitch += input.Y;
	newRot.Yaw += input.X;

	newRot.Pitch = FMath::ClampAngle(newRot.Pitch, cameraClampMin, cameraClampMax);

	viewPivot->SetWorldRotation(newRot);
}

void ARPlayerBase::QuitOut()
{
	APlayerController* PlayerController = GetController<APlayerController>();
	if (PlayerController)
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PlayerController, EQuitPreference::Quit, true);
	}
}

void ARPlayerBase::DoBasicAttack()
{
	GetAbilitySystemComponent()->PressInputID((int)EAbilityInputID::BasicAttack);
}

void ARPlayerBase::EnableScoping()
{
	GetAbilitySystemComponent()->PressInputID((int)EAbilityInputID::Scoping);
}

void ARPlayerBase::DisableScoping()
{
	GetAbilitySystemComponent()->InputCancel();
}

void ARPlayerBase::TryActivateSpecialAttack()
{

}

void ARPlayerBase::TryActivateUltimateAttack()
{

}

void ARPlayerBase::ConfirmActionTriggered()
{
	UE_LOG(LogTemp, Warning, TEXT("Confirmed"));
	//GetAbilitySystemComponent()->InputConfirm();
}

void ARPlayerBase::CancelActionTriggered()
{
	UE_LOG(LogTemp, Warning, TEXT("Cancelled"));
	//GetAbilitySystemComponent()->InputCancel();
}

FVector ARPlayerBase::GetMoveFwdDir() const
{
	FVector CamerFwd = viewCamera->GetForwardVector();
	CamerFwd.Z = 0;
	return CamerFwd.GetSafeNormal();
}

FVector ARPlayerBase::GetMoveRightDir() const
{
	return viewCamera->GetRightVector();
}

void ARPlayerBase::ScopingTagChanged(bool bNewIsAiming)
{
	//bUseControllerRotationYaw = bNewIsAiming;
	bIsScoping = bNewIsAiming;
	GetCharacterMovement()->bOrientRotationToMovement = !bNewIsAiming;

	if (playerController)
		playerController->ChangeCrosshairState(bNewIsAiming);

	if (bNewIsAiming)
	{
		cameraClampMax = 50;
		cameraClampMin = -50;
		LerpCameraToLocalOffset(AimCameraLocalOffset);
	}
	else
	{
		cameraClampMax = 10;
		cameraClampMin = -60;
		LerpCameraToLocalOffset(DefaultCameraLocal);
	}
}

void ARPlayerBase::LerpCameraToLocalOffset(const FVector& LocalOffset)
{
	GetWorldTimerManager().ClearTimer(CameraLerpHandle);
	CameraLerpHandle = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ARPlayerBase::TickCameraLocalOffset, LocalOffset));
}

void ARPlayerBase::TickCameraLocalOffset(FVector Goal)
{
	FVector CurrentLocalOffset = viewCamera->GetRelativeLocation();
	if (FVector::Dist(CurrentLocalOffset, Goal) < 1)
	{
		viewCamera->SetRelativeLocation(Goal);
		return;
	}

	FVector NewLocalOffset = FMath::Lerp(CurrentLocalOffset, Goal, GetWorld()->GetDeltaSeconds() * AimCameraLerpingSpeed);
	viewCamera->SetRelativeLocation(NewLocalOffset);
	CameraLerpHandle = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ARPlayerBase::TickCameraLocalOffset, Goal));
}
