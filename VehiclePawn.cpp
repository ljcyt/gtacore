#include "VehiclePawn.h"

AVehiclePawn::AVehiclePawn()
{
	DriverSeat = CreateDefaultSubobject<USceneComponent>(TEXT("DriverSeat"));
	DriverSeat->SetupAttachment(RootComponent);
}
