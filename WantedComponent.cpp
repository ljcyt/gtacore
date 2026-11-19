#include "WantedComponent.h"

UWantedComponent::UWantedComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

float UWantedComponent::HeatForSeverity(int32 Severity)
{
	switch (Severity)
	{
	case 1:  return 40.f;	// 偷车、撞车逃逸：1 星起步
	case 2:  return 110.f;	// 持械袭击：直接 2 星
	case 3:  return 220.f;	// 杀人、袭警：3 星并留余量
	default: return 20.f;
	}
}

void UWantedComponent::ReportCrime(int32 Severity)
{
	if (State == EWantedState::Busted) return;

	Heat = FMath::Min(Heat + HeatForSeverity(Severity), HeatPerStar * 5.f);
	RecalcStars();

	// 犯了罪但还没被盯上 = Suspected（可以靠躲藏直接消掉）；
	// 追缉中再犯罪只涨星，不改状态。
	if (State == EWantedState::Clean || State == EWantedState::Evading)
	{
		SetState(EWantedState::Suspected);
	}
}

void UWantedComponent::NotifySeenByPolice()
{
	bSeenThisTick = true;
}

void UWantedComponent::NotifyBustProgress(float DeltaTime, bool bOfficerInRange)
{
	if (Stars == 0 || State == EWantedState::Busted) return;

	BustTimer = bOfficerInRange ? BustTimer + DeltaTime : 0.f;
	if (BustTimer >= BustSeconds)
	{
		SetState(EWantedState::Busted); // 没收/重生由 GameMode 监听委托处理
	}
}

void UWantedComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Stars == 0 || State == EWantedState::Busted)
	{
		bSeenThisTick = false;
		return;
	}

	if (bSeenThisTick)
	{
		TimeSinceSeen = 0.f;
		EvadeTimer = 0.f;
		if (State != EWantedState::Pursuit)
		{
			SetState(EWantedState::Pursuit);
		}
	}
	else
	{
		TimeSinceSeen += DeltaTime;

		if (State == EWantedState::Pursuit && TimeSinceSeen >= LoseSightGrace)
		{
			// 警察转入搜索模式：以最后目击点为圆心、GetSearchRadius() 为半径扩散
			SetState(EWantedState::Evading);
		}

		if (State == EWantedState::Evading || State == EWantedState::Suspected)
		{
			EvadeTimer += DeltaTime;
			const float Need = EvadeSecondsPerStar + Stars * 1.5f; // 星越多消得越慢
			if (EvadeTimer >= Need)
			{
				EvadeTimer = 0.f;
				Heat = FMath::Max(0.f, (Stars - 1) * HeatPerStar); // 掉一颗星
				RecalcStars();
				if (Stars == 0)
				{
					Heat = 0.f;
					SetState(EWantedState::Clean);
				}
			}
		}
	}

	bSeenThisTick = false;
}

void UWantedComponent::RecalcStars()
{
	const int32 NewStars = FMath::Clamp(FMath::CeilToInt(Heat / HeatPerStar), 0, 5);
	if (NewStars != Stars)
	{
		Stars = NewStars;
		OnWantedChanged.Broadcast(Stars, State);
	}
}

void UWantedComponent::SetState(EWantedState NewState)
{
	if (State == NewState) return;
	State = NewState;
	OnWantedChanged.Broadcast(Stars, State);
}
