// DriveInteractionComponent.h — 步行/驾驶双态切换（挂在玩家角色上）
// 核心手法和 GTA 一样：上车 = 把角色贴到驾驶座 + 控制权 Possess 给车辆 Pawn；
// 下车 = 在车身左/右/后三处扫一个胶囊体落点，安全才把控制权还给角色。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DriveInteractionComponent.generated.h"

UENUM(BlueprintType)
enum class EPlayerMode : uint8
{
	OnFoot,
	Driving
};

class AVehiclePawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerModeChanged, EPlayerMode, NewMode);

UCLASS(ClassGroup = (GTA), meta = (BlueprintSpawnableComponent))
class GTACORE_API UDriveInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 互动键调用：步行时找最近的空车上车，驾驶时下车 */
	UFUNCTION(BlueprintCallable, Category = "GTA|Drive")
	void TryToggleDrive();

	UPROPERTY(BlueprintReadOnly, Category = "GTA|Drive")
	EPlayerMode Mode = EPlayerMode::OnFoot;

	UPROPERTY(BlueprintReadOnly, Category = "GTA|Drive")
	TObjectPtr<AVehiclePawn> CurrentVehicle;

	UPROPERTY(BlueprintAssignable, Category = "GTA|Drive")
	FOnPlayerModeChanged OnPlayerModeChanged;

	/** 找车半径（cm） */
	UPROPERTY(EditAnywhere, Category = "GTA|Drive")
	float InteractRadius = 350.f;

private:
	void EnterVehicle(AVehiclePawn* Vehicle);
	void ExitVehicle();
	AVehiclePawn* FindNearestVehicle() const;
	bool FindExitSpot(FVector& OutLocation) const;
};
