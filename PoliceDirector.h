// PoliceDirector.h — 警察生成器（世界侧）
// 玩家侧的 WantedComponent 只管星级和状态；"每颗星来多少人、从哪刷、什么时候收队"
// 全归这个 Director。它监听 OnWantedChanged，按星级响应表维持在场警力。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoliceDirector.generated.h"

class UWantedComponent;
class AVehiclePawn;
enum class EWantedState : uint8;

USTRUCT(BlueprintType)
struct FStarResponse
{
	GENERATED_BODY()

	/** 同时在场的警力上限 */
	UPROPERTY(EditAnywhere) int32 MaxUnits = 2;
	/** 补员间隔（秒） */
	UPROPERTY(EditAnywhere) float SpawnInterval = 6.f;
	/** 刷新环带（cm，相对玩家）：太近会穿帮，太远赶不到 */
	UPROPERTY(EditAnywhere) float RingMin = 5000.f;
	UPROPERTY(EditAnywhere) float RingMax = 8000.f;
	/** 路障生成间隔（秒），0 = 不设卡 */
	UPROPERTY(EditAnywhere) float RoadblockInterval = 0.f;
	/** 是否出直升机（持续提供视线，压制消星） */
	UPROPERTY(EditAnywhere) bool bHelicopter = false;
	/** 是否允许撞击/PIT 玩家车辆 */
	UPROPERTY(EditAnywhere) bool bRamAllowed = false;
};

UCLASS()
class GTACORE_API APoliceDirector : public AActor
{
	GENERATED_BODY()

public:
	APoliceDirector();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 星级响应表，下标 = 星级（0 号位空置） */
	UPROPERTY(EditAnywhere, Category = "GTA|Police")
	TArray<FStarResponse> Responses;

	UPROPERTY(EditAnywhere, Category = "GTA|Police")
	TSubclassOf<AVehiclePawn> PoliceCarClass;

	UPROPERTY(EditAnywhere, Category = "GTA|Police")
	TSubclassOf<APawn> HelicopterClass;

	/** 单位离玩家超过这个距离且不在追缉中就回收（cm） */
	UPROPERTY(EditAnywhere, Category = "GTA|Police")
	float DespawnRadius = 12000.f;

private:
	UPROPERTY()
	TObjectPtr<UWantedComponent> PlayerWanted;

	UPROPERTY()
	TObjectPtr<APawn> PlayerPawn;

	TArray<TWeakObjectPtr<APawn>> ActiveUnits;
	TWeakObjectPtr<APawn> HeliUnit;

	float TickAccumulator = 0.f;	// 生成逻辑 0.25s 跑一次，不用每帧
	float SpawnCooldown = 0.f;
	float RoadblockCooldown = 0.f;

	UFUNCTION()
	void HandleWantedChanged(int32 Stars, EWantedState State);

	const FStarResponse* CurrentResponse() const;
	void MaintainPopulation(float DeltaTime);
	bool TrySpawnUnit(const FStarResponse& R);
	bool TrySpawnHelicopter(const FStarResponse& R);
	void CleanupUnits();
	void StandDownAll();

	/** 候选点是否"玩家看得见"：在镜头视锥内且视线无遮挡 → 禁止在此刷新 */
	bool IsPointVisibleToPlayer(const FVector& Point) const;
	/** 把候选点吸附到路面/导航网格（正式地图里换成路网样条吸附） */
	bool SnapToGround(FVector& InOutPoint) const;
};
