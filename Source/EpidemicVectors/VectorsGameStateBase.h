// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"

#include "MutationChar.h"
#include "Grappable.h"

#include "VectorsGameStateBase.generated.h"

class AMutationChar;
/**
 * 
 */
UCLASS()
class EPIDEMICVECTORS_API AVectorsGameStateBase : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;
	void RemoveMutation(int Mutation_i);
		
	UWorld* world;
	//to store all mutations in scene
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<AMutationChar*> mutations;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool motionComicLock;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool grabTeacherUnlock;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int checkpointID;
};
