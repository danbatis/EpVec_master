// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "CustomWaypoint.generated.h"


USTRUCT(BlueprintType)
struct FScanParams {
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float timeInOldHead;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float timeToScan;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float angleToScan;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float timeInMidHead;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float timeToLookNewHead;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float timeBeforeTraverse;

	FScanParams() {
		timeInOldHead = 0.0f;
		timeToScan = 0.0f;
		angleToScan = 0.0f;
		timeInMidHead = 0.0f;
		timeToLookNewHead = 0.0f;
		timeBeforeTraverse = 0.0f;
	}
};

UCLASS()
class EPIDEMICVECTORS_API ACustomWaypoint : public ATargetPoint
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool airGoal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FScanParams scanParams;
};
