#include "DriveInteractionComponent.h"
#include "VehiclePawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

void UDriveInteractionComponent::TryToggleDrive()
{
	if (Mode == EPlayerMode::OnFoot)
	{
		if (AVehiclePawn* Vehicle = FindNearestVehicle())
		{
			EnterVehicle(Vehicle);
		}
	}
	else
	{
		ExitVehicle();
	}
}

AVehiclePawn* UDriveInteractionComponent::FindNearestVehicle() const
{
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	if (!Char) return nullptr;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Char);
	const FVector Center = Char->GetActorLocation();

	GetWorld()->SweepMultiByChannel(Hits, Center, Center, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(InteractRadius), Params);

	AVehiclePawn* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FHitResult& Hit : Hits)
	{
		AVehiclePawn* V = Cast<AVehiclePawn>(Hit.GetActor());
		if (!V || V->bHasDriver) continue; // 有主的车不抢（抢车动画是 M2 的事）
		const float D = FVector::DistSquared(Center, V->GetActorLocation());
		if (D < BestDistSq) { BestDistSq = D; Best = V; }
	}
	return Best;
}

void UDriveInteractionComponent::EnterVehicle(AVehiclePawn* Vehicle)
{
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	if (!Char || !Vehicle) return;
	AController* Ctrl = Char->GetController();
	if (!Ctrl) return;

	Vehicle->bHasDriver = true;
	CurrentVehicle = Vehicle;

	// 1) 角色贴到驾驶座：关碰撞、停移动、挂到座椅附着点
	Char->SetActorEnableCollision(false);
	Char->GetCharacterMovement()->DisableMovement();
	Char->AttachToComponent(Vehicle->DriverSeat,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	// 2) 控制权交给车辆 Pawn（车辆自己读油门/方向/刹车输入）
	Ctrl->Possess(Vehicle);

	Mode = EPlayerMode::Driving;
	OnPlayerModeChanged.Broadcast(Mode);
}

void UDriveInteractionComponent::ExitVehicle()
{
	if (!CurrentVehicle) return;
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	// 注意：此时 Controller 在车辆身上，要从车辆取，不能从角色取
	AController* Ctrl = CurrentVehicle->GetController();
	if (!Char || !Ctrl) return;

	FVector ExitLocation;
	if (!FindExitSpot(ExitLocation)) return; // 三侧全堵死（贴墙/翻车）就不下，留在车里

	// 1) 角色脱离车辆、落到下车点、恢复碰撞与移动
	Char->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Char->SetActorLocation(ExitLocation);
	Char->SetActorEnableCollision(true);
	Char->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	// 2) 控制权还给角色
	Ctrl->Possess(Char);

	CurrentVehicle->bHasDriver = false;
	CurrentVehicle = nullptr;
	Mode = EPlayerMode::OnFoot;
	OnPlayerModeChanged.Broadcast(Mode);
}

bool UDriveInteractionComponent::FindExitSpot(FVector& OutLocation) const
{
	if (!CurrentVehicle) return false;

	const FVector Origin = CurrentVehicle->GetActorLocation();
	const FVector Right = CurrentVehicle->GetActorRightVector();
	const FVector Forward = CurrentVehicle->GetActorForwardVector();

	// 驾驶侧（左）优先，其次右侧，最后车尾
	const FVector Candidates[3] = {
		Origin - Right * 150.f,
		Origin + Right * 150.f,
		Origin - Forward * 220.f
	};

	for (const FVector& C : Candidates)
	{
		// 胶囊体（角色尺寸）不与任何东西重叠才算安全落点
		const bool bBlocked = GetWorld()->OverlapBlockingTestByChannel(
			C + FVector(0.f, 0.f, 90.f), FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(34.f, 88.f));
		if (!bBlocked)
		{
			OutLocation = C;
			return true;
		}
	}
	return false;
}
