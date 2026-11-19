// PoliceBrainComponent.h — 单个警员/警车的追缉判定（挂在警车 Pawn 上）
// 只负责"判定"：看没看见、追哪、搜哪、何时放弃。
// 具体怎么打方向盘由车辆控制器订阅 OnMoveTarget 决定，本组件不碰转向。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PoliceBrainComponent.generated.h"

class UWantedComponent;

UENUM(BlueprintType)
enum class EPoliceState : uint8
{
	Pursue,			// 视线内，直追（带速度预判的截断点）
	GoToLastKnown,	// 刚丢视线，扑向最后已知位置
	Search,			// 已到达最后已知位置，扩散搜索
	StandDown		// 收队，等 Director 回收
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPoliceMoveTarget, FVector, Target, EPoliceState, State);

UCLASS(ClassGroup = (GTA), meta = (BlueprintSpawnableComponent))
class GTACORE_API UPoliceBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPoliceBrainComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Director 刷出单位后调用，注入追缉目标 */
	void InitPursuit(APawn* InPlayer, UWantedComponent* InWanted);

	/** 收队（消星/Busted 时由 Director 广播） */
	void StandDown();

	UPROPERTY(BlueprintReadOnly, Category = "GTA|Police")
	EPoliceState State = EPoliceState::StandDown;

	UPROPERTY(BlueprintReadOnly, Category = "GTA|Police")
	FVector LastKnownPos = FVector::ZeroVector;

	/** 直升机模式：无视地面遮挡，但玩家头顶有遮挡（桥洞/隧道/楼内）时丢视线 */
	UPROPERTY(EditAnywhere, Category = "GTA|Police")
	bool bIsHelicopter = false;

	/** 基础视野半径（cm），实际值按星级放大 */
	UPROPERTY(EditAnywhere, Category = "GTA|Police")
	float BaseSightRadius = 4500.f;

	/** 感知检测间隔（秒）。多单位错峰，避免同帧全员打射线 */
	UPROPERTY(EditAnywhere, Category = "GTA|Police")
	float PerceptionInterval = 0.15f;

	UPROPERTY(BlueprintAssignable, Category = "GTA|Police")
	FOnPoliceMoveTarget OnMoveTarget;

private:
	TWeakObjectPtr<APawn> Player;
	TWeakObjectPtr<UWantedComponent> PlayerWanted;

	float SenseTimer = 0.f;
	float SenseOffset = 0.f;		// BeginPlay 时按实例 ID 错峰
	float TimeSinceSeen = 0.f;
	float SearchTimer = 0.f;
	float SearchPointTimer = 0.f;
	FVector CurrentSearchPoint = FVector::ZeroVector;

	void Sense();					// 一次感知判定（低频）
	bool CanSeePlayer() const;
	FVector ComputePursuitTarget() const;
	void PickNewSearchPoint();
	float SightRadius() const;
	float SearchBudget() const { return 10.f + (PlayerWanted.IsValid() ? PlayerWanted->Stars : 1) * 5.f; }
};
