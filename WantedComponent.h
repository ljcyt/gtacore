// WantedComponent.h — 通缉系统状态机（挂在玩家角色上）
// 警察 AI 只管两件事：看见玩家时调 NotifySeenByPolice()，贴身时调 NotifyBustProgress()。
// 涨星/消星/状态流转全在这个组件里，GameMode 只监听 OnWantedChanged 处理被捕结算。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WantedComponent.generated.h"

UENUM(BlueprintType)
enum class EWantedState : uint8
{
	Clean		UMETA(DisplayName = "无通缉"),
	Suspected	UMETA(DisplayName = "被怀疑"),	// 犯了罪但还没被警员目击确认
	Pursuit		UMETA(DisplayName = "追缉中"),	// 警员视线内
	Evading		UMETA(DisplayName = "脱离视线"),	// 消星倒计时中
	Busted		UMETA(DisplayName = "被捕")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWantedChanged, int32, Stars, EWantedState, State);

UCLASS(ClassGroup = (GTA), meta = (BlueprintSpawnableComponent))
class GTACORE_API UWantedComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWantedComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 犯罪上报。Severity: 1=偷车/撞车逃逸 2=持械袭击 3=杀人/袭警 */
	UFUNCTION(BlueprintCallable, Category = "GTA|Wanted")
	void ReportCrime(int32 Severity);

	/** 警员 AI 在视线/感知范围内看见玩家时，每帧调用 */
	UFUNCTION(BlueprintCallable, Category = "GTA|Wanted")
	void NotifySeenByPolice();

	/** 警员贴身抓捕：bOfficerInRange 为真时累积抓捕进度，满了触发 Busted */
	UFUNCTION(BlueprintCallable, Category = "GTA|Wanted")
	void NotifyBustProgress(float DeltaTime, bool bOfficerInRange);

	UFUNCTION(BlueprintPure, Category = "GTA|Wanted")
	bool IsWanted() const { return Stars > 0; }

	/** 警察生成器按星级取搜索半径（cm）：星越多，包围圈越大 */
	UFUNCTION(BlueprintPure, Category = "GTA|Wanted")
	float GetSearchRadius() const { return 3000.f + Stars * 2500.f; }

	UPROPERTY(BlueprintReadOnly, Category = "GTA|Wanted")
	int32 Stars = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GTA|Wanted")
	EWantedState State = EWantedState::Clean;

	UPROPERTY(BlueprintAssignable, Category = "GTA|Wanted")
	FOnWantedChanged OnWantedChanged;

	// —— 可调参数（蓝图里直接改） ——

	/** 每颗星需要的热度 */
	UPROPERTY(EditAnywhere, Category = "GTA|Wanted")
	float HeatPerStar = 100.f;

	/** 脱离视线多久后转入 Evading（秒） */
	UPROPERTY(EditAnywhere, Category = "GTA|Wanted")
	float LoseSightGrace = 3.f;

	/** 消一颗星的基础躲避时长（秒），实际 = 本值 + 星数 * 1.5 */
	UPROPERTY(EditAnywhere, Category = "GTA|Wanted")
	float EvadeSecondsPerStar = 6.f;

	/** 抓捕圈内站满几秒被捕 */
	UPROPERTY(EditAnywhere, Category = "GTA|Wanted")
	float BustSeconds = 2.5f;

private:
	float Heat = 0.f;			// 热度池，Stars = ceil(Heat / HeatPerStar)，封顶 5
	float TimeSinceSeen = 0.f;	// 距上次被警员看见的时间
	float EvadeTimer = 0.f;		// 消星计时
	float BustTimer = 0.f;		// 抓捕计时
	bool bSeenThisTick = false;	// 本帧是否有警员看见（Tick 末尾清零）

	static float HeatForSeverity(int32 Severity);
	void SetState(EWantedState NewState);
	void RecalcStars();
};
