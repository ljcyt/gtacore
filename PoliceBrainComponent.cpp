#include "PoliceBrainComponent.h"
#include "WantedComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

UPoliceBrainComponent::UPoliceBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPoliceBrainComponent::InitPursuit(APawn* InPlayer, UWantedComponent* InWanted)
{
	Player = InPlayer;
	PlayerWanted = InWanted;
	// 按实例 ID 错峰：12 个单位的感知射线均匀摊到 5 个相位里，不在同一帧爆发
	SenseOffset = (GetOwner()->GetUniqueID() % 5) * (PerceptionInterval / 5.f);
	SenseTimer = SenseOffset;
	if (InPlayer)
	{
		LastKnownPos = InPlayer->GetActorLocation();
	}
	State = EPoliceState::GoToLastKnown;
}

void UPoliceBrainComponent::StandDown()
{
	State = EPoliceState::StandDown;
	OnMoveTarget.Broadcast(GetOwner()->GetActorLocation(), State);
}

float UPoliceBrainComponent::SightRadius() const
{
	const int32 Stars = PlayerWanted.IsValid() ? PlayerWanted->Stars : 1;
	return BaseSightRadius + Stars * 500.f; // 星越高看得越远（增援带情报共享的手感来源）
}

bool UPoliceBrainComponent::CanSeePlayer() const
{
	APawn* P = Player.Get();
	APawn* Self = Cast<APawn>(GetOwner());
	if (!P || !Self) return false;

	const FVector Eye = Self->GetActorLocation() + FVector(0.f, 0.f, 150.f);
	const FVector Target = P->GetActorLocation() + FVector(0.f, 0.f, 100.f);
	if (FVector::Dist(Eye, Target) > SightRadius()) return false;

	if (bIsHelicopter)
	{
		// 直升机无视地面遮挡，唯一克星是头顶遮挡：桥洞、隧道、室内
		FHitResult CoverHit;
		const bool bCovered = GetWorld()->LineTraceSingleByChannel(CoverHit,
			P->GetActorLocation(), P->GetActorLocation() + FVector(0.f, 0.f, 1500.f),
			ECC_Visibility);
		return !bCovered;
	}

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Self);
	Params.AddIgnoredActor(P);
	// 射线打到玩家本体才算看见；中途命中任何建筑就是没看见
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params);
	return !bBlocked;
}

void UPoliceBrainComponent::Sense()
{
	if (!Player.IsValid() || State == EPoliceState::StandDown) return;

	if (CanSeePlayer())
	{
		TimeSinceSeen = 0.f;
		LastKnownPos = Player->GetActorLocation();
		State = EPoliceState::Pursue;
		// 关键回传：刷新玩家侧的"被看见"，通缉衰减计时被清零
		if (PlayerWanted.IsValid())
		{
			PlayerWanted->NotifySeenByPolice();
		}
		return;
	}

	TimeSinceSeen += PerceptionInterval;

	switch (State)
	{
	case EPoliceState::Pursue:
		if (TimeSinceSeen >= 2.f) // 丢视线 2 秒才降级，短暂遮挡（过个路口）不算甩掉
		{
			State = EPoliceState::GoToLastKnown;
			SearchTimer = 0.f;
		}
		break;

	case EPoliceState::GoToLastKnown:
		if (FVector::Dist2D(GetOwner()->GetActorLocation(), LastKnownPos) < 400.f)
		{
			State = EPoliceState::Search;
			SearchTimer = 0.f;
			SearchPointTimer = 0.f;
			PickNewSearchPoint();
		}
		break;

	case EPoliceState::Search:
		SearchTimer += PerceptionInterval;
		if (SearchTimer >= SearchBudget()) // 搜满预算还没找到 → 放弃，等 Director 回收
		{
			StandDown();
		}
		break;

	default:
		break;
	}
}

FVector UPoliceBrainComponent::ComputePursuitTarget() const
{
	APawn* P = Player.Get();
	if (!P) return LastKnownPos;
	if (State != EPoliceState::Pursue) return LastKnownPos;

	// 截断点 = 玩家位置 + 玩家速度 * 预判时间。
	// 预判时间按距离/自身速度估算并钳制——不做预判的 AI 只会永远跟在车尾，
	// 玩家一个直角弯就甩掉了，这是新手追缉 AI 最典型的死法。
	const float Dist = FVector::Dist(GetOwner()->GetActorLocation(), P->GetActorLocation());
	const float Lead = FMath::Clamp(Dist / 1500.f, 0.f, 1.2f);
	return P->GetActorLocation() + P->GetVelocity() * Lead;
}

void UPoliceBrainComponent::PickNewSearchPoint()
{
	// 搜索点在最后已知位置周围扩散，半径随搜索时长膨胀（模拟包围圈外扩）
	const float Radius = 800.f + SearchTimer * 120.f;
	const float Angle = FMath::RandRange(0.f, 2.f * PI);
	CurrentSearchPoint = LastKnownPos + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
}

void UPoliceBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (State == EPoliceState::StandDown) return;

	SenseTimer += DeltaTime;
	if (SenseTimer >= PerceptionInterval)
	{
		SenseTimer = 0.f;
		Sense();
	}

	// 移动目标每帧给（便宜），感知判定低频跑（贵），两者分开
	switch (State)
	{
	case EPoliceState::Pursue:
		OnMoveTarget.Broadcast(ComputePursuitTarget(), State);
		break;
	case EPoliceState::GoToLastKnown:
		OnMoveTarget.Broadcast(LastKnownPos, State);
		break;
	case EPoliceState::Search:
		SearchPointTimer += DeltaTime;
		if (SearchPointTimer >= 4.f || FVector::Dist2D(GetOwner()->GetActorLocation(), CurrentSearchPoint) < 300.f)
		{
			SearchPointTimer = 0.f;
			PickNewSearchPoint();
		}
		OnMoveTarget.Broadcast(CurrentSearchPoint, State);
		break;
	default:
		break;
	}
}
