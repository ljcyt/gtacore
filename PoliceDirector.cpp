#include "PoliceDirector.h"
#include "WantedComponent.h"
#include "VehiclePawn.h"
#include "PoliceBrainComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

APoliceDirector::APoliceDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	// 默认响应表：下标 = 星级。数值是手感调出来的，不是拍脑袋——
	// 1 星只是"跟上你"，3 星开始设卡，4 星上直升机锁视线，5 星才准撞。
	Responses.SetNum(6);
	Responses[1] = { 2, 6.0f, 5000.f, 8000.f, 0.f,  false, false };
	Responses[2] = { 4, 5.0f, 5000.f, 8000.f, 0.f,  false, false };
	Responses[3] = { 6, 4.0f, 4500.f, 7500.f, 20.f, false, true  };
	Responses[4] = { 8, 3.5f, 4500.f, 7000.f, 15.f, true,  true  };
	Responses[5] = { 12, 3.0f, 4000.f, 7000.f, 10.f, true,  true  };
}

void APoliceDirector::BeginPlay()
{
	Super::BeginPlay();
	PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn)
	{
		PlayerWanted = PlayerPawn->FindComponentByClass<UWantedComponent>();
		if (PlayerWanted)
		{
			PlayerWanted->OnWantedChanged.AddDynamic(this, &APoliceDirector::HandleWantedChanged);
		}
	}
}

void APoliceDirector::HandleWantedChanged(int32 Stars, EWantedState State)
{
	if (Stars == 0)
	{
		StandDownAll(); // 消星成功：全员收队，出玩家视野后销毁
	}
}

const FStarResponse* APoliceDirector::CurrentResponse() const
{
	if (!PlayerWanted || PlayerWanted->Stars <= 0) return nullptr;
	return &Responses[FMath::Clamp(PlayerWanted->Stars, 1, Responses.Num() - 1)];
}

void APoliceDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickAccumulator += DeltaTime;
	if (TickAccumulator < 0.25f) return; // 生成/回收都是低频逻辑
	const float Step = TickAccumulator;
	TickAccumulator = 0.f;

	CleanupUnits();
	if (const FStarResponse* R = CurrentResponse())
	{
		MaintainPopulation(Step);
	}
}

void APoliceDirector::MaintainPopulation(float DeltaTime)
{
	const FStarResponse& R = *CurrentResponse();
	SpawnCooldown -= DeltaTime;

	if (R.bHelicopter && !HeliUnit.IsValid())
	{
		TrySpawnHelicopter(R);
	}

	if (ActiveUnits.Num() < R.MaxUnits && SpawnCooldown <= 0.f)
	{
		if (TrySpawnUnit(R))
		{
			SpawnCooldown = R.SpawnInterval;
		}
		else
		{
			SpawnCooldown = 1.f; // 这轮没找到合法刷新点，1 秒后再试，别死循环采样
		}
	}
}

bool APoliceDirector::TrySpawnUnit(const FStarResponse& R)
{
	if (!PoliceCarClass || !PlayerPawn) return false;
	const FVector PlayerLoc = PlayerPawn->GetActorLocation();

	// 采样 8 个候选点，命中一个合法的就刷，全不合法这轮放弃
	for (int32 i = 0; i < 8; ++i)
	{
		const float Angle = FMath::RandRange(0.f, 2.f * PI);
		const float Dist = FMath::RandRange(R.RingMin, R.RingMax);
		FVector Point = PlayerLoc + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Dist;

		if (IsPointVisibleToPlayer(Point)) continue;	// 绝不在玩家眼前刷人（穿帮重灾区）
		if (!SnapToGround(Point)) continue;				// 不在路面/导航区内，刷了也过不来

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AVehiclePawn* Unit = GetWorld()->SpawnActor<AVehiclePawn>(PoliceCarClass, Point,
			(PlayerLoc - Point).Rotation(), Params);
		if (!Unit) continue;

		if (UPoliceBrainComponent* Brain = Unit->FindComponentByClass<UPoliceBrainComponent>())
		{
			Brain->InitPursuit(PlayerPawn, PlayerWanted);
		}
		ActiveUnits.Add(Unit);
		return true;
	}
	return false;
}

bool APoliceDirector::TrySpawnHelicopter(const FStarResponse& R)
{
	if (!HelicopterClass || !PlayerPawn) return false;
	const FVector Above = PlayerPawn->GetActorLocation() + FVector(0.f, 0.f, 6000.f);
	APawn* Heli = GetWorld()->SpawnActor<APawn>(HelicopterClass, Above, FRotator::ZeroRotator);
	if (!Heli) return false;
	if (UPoliceBrainComponent* Brain = Heli->FindComponentByClass<UPoliceBrainComponent>())
	{
		Brain->bIsHelicopter = true;
		Brain->InitPursuit(PlayerPawn, PlayerWanted);
	}
	HeliUnit = Heli;
	ActiveUnits.Add(Heli);
	return true;
}

void APoliceDirector::CleanupUnits()
{
	if (!PlayerPawn) return;
	const FVector PlayerLoc = PlayerPawn->GetActorLocation();

	for (int32 i = ActiveUnits.Num() - 1; i >= 0; --i)
	{
		APawn* Unit = ActiveUnits[i].Get();
		if (!Unit)
		{
			ActiveUnits.RemoveAt(i);
			continue;
		}
		const bool bTooFar = FVector::Dist(PlayerLoc, Unit->GetActorLocation()) > DespawnRadius;
		// 远 + 玩家看不见才销毁；正在追缉的单位不回收，免得追着追着凭空消失
		if (bTooFar && !IsPointVisibleToPlayer(Unit->GetActorLocation()))
		{
			UPoliceBrainComponent* Brain = Unit->FindComponentByClass<UPoliceBrainComponent>();
			if (!Brain || Brain->State == EPoliceState::StandDown || !PlayerWanted || PlayerWanted->Stars == 0)
			{
				Unit->Destroy();
				ActiveUnits.RemoveAt(i);
			}
		}
	}
}

void APoliceDirector::StandDownAll()
{
	for (TWeakObjectPtr<APawn>& Weak : ActiveUnits)
	{
		if (APawn* Unit = Weak.Get())
		{
			if (UPoliceBrainComponent* Brain = Unit->FindComponentByClass<UPoliceBrainComponent>())
			{
				Brain->StandDown();
			}
		}
	}
}

bool APoliceDirector::IsPointVisibleToPlayer(const FVector& Point) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) return false;

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamFwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector ToPoint = Point - CamLoc;

	// 视锥外（>55°）直接算看不见，不用打射线，省性能
	if (FVector::DotProduct(CamFwd, ToPoint.GetSafeNormal()) < 0.57f) return false;

	// 视锥内再补一发遮挡射线：被楼挡住的点同样可以刷
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(PlayerPawn);
	return !GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, Point, ECC_Visibility, Params);
}

bool APoliceDirector::SnapToGround(FVector& InOutPoint) const
{
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!Nav) return false;
	FNavLocation Out;
	// 正式地图里换成路网样条吸附；导航投影是灰盒阶段的等价物
	if (!Nav->ProjectPointToNavigation(InOutPoint, Out, FVector(500.f, 500.f, 1000.f))) return false;
	InOutPoint = Out.Location + FVector(0.f, 0.f, 50.f);
	return true;
}
