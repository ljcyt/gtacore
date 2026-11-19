// VehiclePawn.h — 可驾驶车辆基类
// 车辆物理（Chaos Vehicle Movement Component）和输入在子类里接，
// 本类只提供上下车系统需要的契约：驾驶座附着点 + 占用标记。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VehiclePawn.generated.h"

UCLASS(Blueprintable)
class GTACORE_API AVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AVehiclePawn();

	/** 驾驶座附着点（在蓝图里摆到座椅位置） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GTA|Drive")
	TObjectPtr<USceneComponent> DriverSeat;

	/** 是否已有驾驶员（AI/玩家共用同一把锁，防抢座） */
	UPROPERTY(BlueprintReadWrite, Category = "GTA|Drive")
	bool bHasDriver = false;
};
