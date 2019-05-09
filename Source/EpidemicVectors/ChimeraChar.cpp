// Fill out your copyright notice in the Description page of Project Settings.

#include "ChimeraChar.h"

void AChimeraChar::NextComboHit() {
	if (distToTarget < strikeDistance) {
		//Decide if following up or just restarting attacks
		//if (FMath::RandRange(0.0f, 1.0f) < aggressivity) {
		if (!attackConnected) {
			ResetFightAnims();
		}
		else {
			//forcing to go to the left because we lack the animations for the right
			if (FMath::RandRange(0.0f, 1.0f) < 0.5f) {
				if (atkWalker->right) {
					atkWalker = atkWalker->right;
					MeleeAttack();
				}
				else { ResetFightAnims(); }
			}
			else {
				if (atkWalker->left) {
					atkWalker = atkWalker->left;
					MeleeAttack();
				}
				else { ResetFightAnims(); }
			}
		}
	}
	else {
		ResetFightAnims();
	}
}

void AChimeraChar::Death() {
	const FVector spawnPosition = GetActorLocation();

	Super::Death();

	//instantiate final comicplayer
	FActorSpawnParameters spawnInfo;
	spawnInfo.Owner = this;

	AActor * newActor;
	newActor = GetWorld()->SpawnActor<AActor>(finalComicPlayer, spawnPosition, FRotator::ZeroRotator, spawnInfo);
}

