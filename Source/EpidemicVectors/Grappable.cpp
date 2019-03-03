// Fill out your copyright notice in the Description page of Project Settings.

#include "Grappable.h"
#include "EpidemicVectors.h"

AGrappable::AGrappable() {
	myMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("myMesh"));
	RootComponent = myMesh;
}
void AGrappable::BeginPlay() {
	
	//GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White, "rootName: " + RootComponent->GetName());

}
void AGrappable::Tick(float DeltaTime) {}
