// Fill out your copyright notice in the Description page of Project Settings.

#include "VectorsGameStateBase.h"

void AVectorsGameStateBase::BeginPlay()
{
	Super::BeginPlay();

	world = GetWorld();

	mutations.Empty();
	int mutation_i = 0;

	//find enemies
	for (TActorIterator<AMutationChar> itr(world, AMutationChar::StaticClass()); itr; ++itr)
	{
		AMutationChar* mutation = *itr;
		if (mutation != NULL) {
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, "Mutation found name: " + mutation->GetName());

			mutations.Add(mutation);
			mutation->mutation_i = mutation_i;
			mutation_i++;
		}
	}
}

void AVectorsGameStateBase::RemoveMutation(int Mutation_i){
	//if in the middle, update the others
	if(Mutation_i < mutations.Num()-1)
	{
		for (int i = Mutation_i+1; i < mutations.Num(); ++i) {
			mutations[i]->mutation_i = mutations[i]->mutation_i - 1;
		}
	}
	mutations.RemoveAt(Mutation_i,1,true);
}
