// Fill out your copyright notice in the Description page of Project Settings.

#include "MutationChar.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h"

// Sets default values
AMutationChar::AMutationChar()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//Initializing our Pawn Sensing comp and our behavior tree reference
	PawnSensingComp = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("PawnSensingComp"));
	BehaviorTree = CreateDefaultSubobject<UBehaviorTree>(TEXT("BehaviorTreeReference"));
	
	//Don't collide with camera to keep 3rd person camera at position when obstructed by this char
	myCapsuleComp = GetCapsuleComponent();
	//myCapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	//GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
	/*
	//Damage detector
	damageDetector = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageDetector"));
	damageDetector->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	damageDetector->SetupAttachment(myCapsuleComp);
	damageDetector->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	*/	
}

// Called when the game starts or when spawned
void AMutationChar::BeginPlay()
{
	Super::BeginPlay();
		
	EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("MutationStates"), true);
	RayParams.bTraceComplex = false;
	RayParams.bReturnPhysicalMaterial = false;
	//RayParams.AddIgnoredActor(this);
	
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &AMutationChar::OnBeginOverlap);
	GetCapsuleComponent()->OnComponentEndOverlap.AddDynamic(this, &AMutationChar::OnEndOverlap);

	if (PawnSensingComp)
	{
		//Registering the delegate which will fire when we hear something
		PawnSensingComp->OnHearNoise.AddDynamic(this, &AMutationChar::OnHearNoise);
		PawnSensingComp->OnSeePawn.AddDynamic(this, &AMutationChar::OnSeenTarget);
	}

	//update links in attackTree
	for (int i = 0; i < attackList.Num(); ++i) {
		//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::White, "animName: " + attackTreeL[i].myAnim->GetFName().GetPlainNameString());
		if (attackList[i].leftNode != 0) {
			attackList[i].left = &attackList[attackList[i].leftNode];
			//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::White, "updated left leg");
		}
		if (attackList[i].rightNode != 0) {
			attackList[i].right = &attackList[attackList[i].rightNode];
			//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::White, "updated right leg");
		}
	}
	
	atkWalker = &attackList[0];
	myMesh = GetMesh();
	myAnimBP = Cast<UMutationAnimComm>(myMesh->GetAnimInstance());
	if(myMesh->GetChildComponent(0)) {
		mainWeaponComp = myMesh->GetChildComponent(0);
		Cast<UPrimitiveComponent>(mainWeaponComp)->SetGenerateOverlapEvents(false);
	}
	else{
		GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Red, FString::Printf(TEXT("[mutation] missing children attack mesh!!!")));
	}

	positionOffset = myMesh->GetRelativeTransform().GetLocation();
	rotationOffset = myMesh->GetRelativeTransform().GetRotation();

	myController = Cast<AMutationCtrl>(GetController());
	myWorld = GetWorld();
	mystate = MutationStates::patrol;
		
	//to enable tests on the editor	
	flying = startAirborne;
		
	myGameState = Cast<AVectorsGameStateBase>(myWorld->GetGameState());

	myCharMove = GetCharacterMovement();
	if (startAirborne) {
		myCharMove->MovementMode = MOVE_Flying;
	}
	else {
		myCharMove->MovementMode = MOVE_Walking;
	}
	myCharMove->MaxWalkSpeed = normalSpeed;
	myCharMove->MaxAcceleration = normalAcel;

	/*
	myController->SetDesperate(life < desperateLifeLevel);
	myController->SetCanFly(canFly);
	myController->SetAirborne(startAirborne);
	myController->SetGoalInAir(false);
	myController->SetDonePath(true);
	*/

	StartPatrol();

	/*
	if (!patrolPoints.IsValidIndex(0)) {
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("[Mutation %s] have no patrol points!!!!"), *GetName()));
		UGameplayStatics::SetGlobalTimeDilation(myWorld, .2f);
	}
	if (&attackList[0] == nullptr) {
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("[Mutation %s] have no attack animation list!!!!"), *GetName()));
		UGameplayStatics::SetGlobalTimeDilation(myWorld, .2f);
	}
	*/
	
}

// Called every frame
void AMutationChar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
		
	mytime += DeltaTime;
	if (searchTimer > 0.0f) {
		searchTimer -= DeltaTime;
		if (searchTimer <= 0.0f) {
			searchTimer = 0.0f;
			if(mystate == MutationStates::pursuit) {
				UE_LOG(LogTemp, Warning, TEXT("[mutation] gave up pursuit, going back to patrol"));
				mystate = MutationStates::patrol;
				targetPos = patrolPoints[nextPatrol_i]->GetActorLocation();
				distToTarget = FVector::Distance(myLoc, targetPos);
				currentScanParams = patrolPoints[nextPatrol_i]->scanParams;
			}
		}
	}
	if (debugInfo) {
		APlayerController* fpPlayer = UGameplayStatics::GetPlayerController(myWorld, 0);
		if (fpPlayer->WasInputKeyJustPressed(EKeys::H)) {
			myWorld->GetTimerManager().ClearTimer(timerHandle);
			heightRollMid = false;
			GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::ClearHeightRoll, 0.3f, false);
			//do a height Roll
			mystate = MutationStates::heightRoll;
			myCharMove->bOrientRotationToMovement = false;
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("[Mutation %s] doing height roll"), *GetName()));
		}
		FString meuestado = EnumPtr->GetNameByValue((int64)mystate).ToString();
		FString moveRes;
		if (movingRes == EPathFollowingRequestResult::RequestSuccessful) {
			//UE_LOG(LogTemp, Warning, TEXT("[mutation] moving success"));
			moveRes = "move success";
		}
		else {
			if (movingRes == EPathFollowingRequestResult::Failed) {
				//UE_LOG(LogTemp, Warning, TEXT("[mutation] moving fail")); 
				moveRes = "move fail";
			}
			else {
				//UE_LOG(LogTemp, Warning, TEXT("[mutation] moving already at goal"));
				moveRes = "move at goal";
			}
		}
		GEngine->AddOnScreenDebugMessage(-1, 0.01f, FColor::Green, FString::Printf(TEXT("[Mutation %s] %s life: %f %s"), *GetName(), *meuestado, life, *moveRes));
		//DrawDebugSphere(myWorld, GetActorLocation(), strikeDistance, 12, FColor::Yellow, true, 0.1f, 0, 1.0);
		//DrawDebugSphere(myWorld, GetActorLocation(), 2*strikeDistance, 12, FColor::Yellow, true, 0.1f, 0, 1.0);
		//DrawDebugSphere(myWorld, GetActorLocation(), fightRange, 12, FColor::Green, true, 0.1f, 0, 1.0);

		if (fpPlayer->WasInputKeyJustPressed(EKeys::I)) {
			UE_LOG(LogTemp, Warning, TEXT("[mutation] became desperate!"));
			mystate = MutationStates::escape;
			myController->SetNeedHideSpot(true);
			waitEQS = true;
		}

		if (fpPlayer->WasInputKeyJustPressed(EKeys::J)) {
			UE_LOG(LogTemp, Warning, TEXT("[mutation] getting a new hide spot"));
			myController->SetNeedHideSpot(true);
			waitEQS = true;
		}
		if (fpPlayer->WasInputKeyJustPressed(EKeys::K)) {
			UE_LOG(LogTemp, Warning, TEXT("[mutation] getting a new seek spot"));
			myController->SetNeedSeekSpot(true);
			waitEQS = true;
		}
		if (fpPlayer->WasInputKeyJustPressed(EKeys::L)) {
			UE_LOG(LogTemp, Warning, TEXT("[mutation] getting a new unobstructed spot"));

			UE_LOG(LogTemp, Warning, TEXT("blackboard updated with target position"));
			myController->SetGoal(targetPos);

			myController->SetNeedBestPath(true);
			waitEQS = true;
		}

		DrawDebugLine(myWorld, GetActorLocation(), targetPos, FColor::Green, true, 0.1f, 0, 5.0);
		FVector algozDir = myLoc - targetPos;
		algozDir.Normalize();
		if (debugInfo) {
			DrawDebugLine(myWorld, targetPos, targetPos + moveTolerance * algozDir, FColor::Black, true, 0.1f, 0, 5.0);
		}
	}

	if (waitEQS) {
		UE_LOG(LogTemp, Warning, TEXT("[mutation] waiting for EQS"));
		if (!myController->GetNeedHideSpot() && !myController->GetNeedSeekSpot() && !myController->GetNeedBestPath()) {
			targetPos = myController->GetGoal();
			myController->RestartBT();
			distToTarget = FVector::Distance(myLoc, targetPos);
			if (debugInfo) {
				DrawDebugLine(myWorld, targetPos, targetPos + 1000.0f*FVector::UpVector, FColor::Black, true, 1.1f, 0, 5.0);

				DrawDebugLine(myWorld, myLoc, myLoc + 1000.0f*FVector::UpVector, FColor::Magenta, true, 1.1f, 0, 5.0);
			}
			waitEQS = false;
			StartTraverse();
		}
	}

	inAir = GetMovementComponent()->IsFalling() || flying;
	if(!inAir && mystate == MutationStates::kdFlight && recoilPortion == 0.0f){
		if (!kdrecovering) {
			kdrecovering = true;
			//hit the ground
			mystate = MutationStates::kdGround;
			myAnimBP->damage = 11;
			//start delayed rise
			float hitgroundTime = 0.5f;
			GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::DelayedKDRise, hitgroundTime, false);
		}
	}	
	myAnimBP->inAir = inAir;
	/*
	if (inAir) {
		UE_LOG(LogTemp, Warning, TEXT("mutation falling"));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("mutation not falling"));
	}
	*/
	
	
	/*
	if (targetVisible) {
		RayParams.AddIgnoredActor(this);		
		targetPos = myTarget->GetActorLocation();
		distToTarget = FVector::Distance(myLoc, targetPos);
		if (myWorld->LineTraceSingleByChannel(hitres, myLoc, targetPos, ECC_MAX, RayParams)) {
			if (debugInfo) {
				DrawDebugLine(myWorld, myLoc, targetPos, FColor::Red, true, 1.1f, 0, 5.0);
			}
			AMyPlayerCharacter * playerChar = Cast<AMyPlayerCharacter>(hitres.Actor.Get());
			if (playerChar) {
				//target still visible
				//targetVisible = true;
				if (distToTarget > fightRange) {
					GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("needs to get closer to %s"), *hitres.Actor.Get()->GetName()));
					//fly straight
					flying = true;
					myCharMove->MovementMode = MOVE_Flying;
					//AddMovementInput(targetPos - myLoc, 1.0f);
					movingRes = myController->MoveToLocation(targetPos, moveTolerance, true, false, false, true, 0, true);
					if (movingRes == EPathFollowingRequestResult::RequestSuccessful) {
						UE_LOG(LogTemp, Warning, TEXT("[mutation] moving success"));
					}
					else {
						if (movingRes == EPathFollowingRequestResult::Failed) { UE_LOG(LogTemp, Warning, TEXT("[mutation] moving fail")); }
						else {
							UE_LOG(LogTemp, Warning, TEXT("[mutation] moving already at goal"));
						}
					}
				}
				else {
					GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("time to fight %s"), *hitres.Actor.Get()->GetName()));
					MutationFight(DeltaTime);
				}
			}
			else{
				//target no longer visible!
				targetVisible = false;
				//turn sensing back on
				PawnSensingComp->SetActive(true);
				PawnSensingComp->SetSensingUpdatesEnabled(true);
				PawnSensingComp->bSeePawns = true;
				PawnSensingComp->bHearNoises = true;

				//start the search
				DrawDebugLine(myWorld, targetPos, targetPos + 1000.0f*FVector::UpVector, FColor::Yellow, true, 1.0f, 0, 5.0);
				waitEQS = false;
				mystate = MutationStates::pursuit;
			}
		}
		else {
			//raycast failed to see anything
			DrawDebugLine(myWorld, myLoc, myLoc+1000.0f*FVector::UpVector, FColor::Red, true, 1.1f, 0, 5.0);
			targetVisible = false;
		}	
	}
	else {
		//target not visible
		DrawDebugSphere(myWorld, myLoc, fightRange, 12, FColor::Red, true, 0.1f, 0, 5.0);

		if (waitEQS) {
			UE_LOG(LogTemp, Warning, TEXT("[mutation] waiting for EQS"));
			if (!myController->GetNeedHideSpot() && !myController->GetNeedSeekSpot() && !myController->GetNeedBestPath()){
				targetPos = myController->GetGoal();
				distToTarget = FVector::Distance(myLoc, targetPos);
				DrawDebugLine(myWorld, targetPos, targetPos + 1000.0f*FVector::UpVector, FColor::Black, true, 1.1f, 0, 5.0);
				waitEQS = false;
				askedBestPath++;
			}
		}
		else {
			myCharMove->bOrientRotationToMovement = true;
			//check if arrived at destination			
			if (distToTarget <= moveTolerance) {
				askedBestPath = 0;
				myController->StopMovement();
				if (mystate == MutationStates::pursuit) {
					//arrived at last known location, ask for a new hiding spot
					myController->SetNeedSeekSpot(true);
					myController->SetGoal(targetPos);
					UE_LOG(LogTemp, Warning, TEXT("[mutation] asked for a seek spot"));
					waitEQS = true;
					if (searchTimer == 0.0f)
						searchTimer = blindPursuitTime;
				}
				if (mystate == MutationStates::patrol) {
					//go to next patrol point
					NextPatrolPoint();
					targetPos = patrolPoints[nextPatrol_i]->GetActorLocation();
					distToTarget = FVector::Distance(myLoc, targetPos);
					currentScanParams = patrolPoints[nextPatrol_i]->scanParams;
				}
			}
			else {
				//keep searching
				//verify if line of sight is clear
				if (canFly) {
					RayParams.AddIgnoredActor(this);
					if (myWorld->LineTraceSingleByChannel(hitres, myLoc, targetPos, ECC_MAX, RayParams)) {
						//check if detected object is the player
						AMyPlayerCharacter * playerChar = Cast<AMyPlayerCharacter>(hitres.Actor.Get());
						if (playerChar) {
							//fly straight
							//UE_LOG(LogTemp, Warning, TEXT("[mutation] flying straight to player"));
							flying = true;
							myCharMove->MovementMode = MOVE_Flying;
							//AddMovementInput(targetPos - myLoc, 1.0f);
							movingRes = myController->MoveToLocation(targetPos, moveTolerance, true, false, false, true, 0, true);
							if (movingRes == EPathFollowingRequestResult::RequestSuccessful) {
								UE_LOG(LogTemp, Warning, TEXT("[mutation] moving success"));
							}
							else {
								if (movingRes == EPathFollowingRequestResult::Failed) { UE_LOG(LogTemp, Warning, TEXT("[mutation] moving fail")); }
								else {
									UE_LOG(LogTemp, Warning, TEXT("[mutation] moving already at goal"));
								}
							}
						}
						else {
							//ask for a best path point a number of times
							if (askedBestPath <= 4) {
								myController->SetGoal(targetPos);
								myController->SetNeedBestPath(true);
								waitEQS = true;
								UE_LOG(LogTemp, Warning, TEXT("[mutation] asked for a best path"));
							}
							else {
								//choose a random direction to try to have a clear line of sight
								UE_LOG(LogTemp, Warning, TEXT("[mutation] choosing random dir"));
								flying = true;
								myCharMove->MovementMode = MOVE_Flying;
								FVector randDest = targetPos + FMath::RandRange(0.0f, 500.0f)*FVector::RightVector + FMath::RandRange(0.0f, 500.0f)*FVector::ForwardVector;
								DrawDebugLine(myWorld, randDest, randDest + 1000.0f*FVector::UpVector, FColor::Magenta, true, 1.1f, 0, 5.0);
								movingRes = myController->MoveToLocation(randDest, moveTolerance, true, false, false, true, 0, true);
								//AddMovementInput(randDest-myLoc, 1.0f);
								if (movingRes == EPathFollowingRequestResult::RequestSuccessful) {
									UE_LOG(LogTemp, Warning, TEXT("[mutation] moving success"));
								}
								else {
									if (movingRes == EPathFollowingRequestResult::Failed) { UE_LOG(LogTemp, Warning, TEXT("[mutation] moving fail")); }
									else {
										UE_LOG(LogTemp, Warning, TEXT("[mutation] moving already at goal"));
									}
								}
							}
						}						
					}
					else {
						//fly straight
						//UE_LOG(LogTemp, Warning, TEXT("[mutation] flying straight to %d"), nextPatrol_i);
						flying = true;
						myCharMove->MovementMode = MOVE_Flying;
						//AddMovementInput(targetPos - myLoc, 1.0f);
						movingRes = myController->MoveToLocation(targetPos, moveTolerance, true, false, false, true, 0, true);
						if (movingRes == EPathFollowingRequestResult::RequestSuccessful) {
							UE_LOG(LogTemp, Warning, TEXT("[mutation] moving success"));
						}
						else {
							if (movingRes == EPathFollowingRequestResult::Failed) { UE_LOG(LogTemp, Warning, TEXT("[mutation] moving fail")); }
							else {
								UE_LOG(LogTemp, Warning, TEXT("[mutation] moving already at goal"));
							}
						}
						//slow down						
						if (distToTarget <= flyStopFactor * moveTolerance) {

							float deacelRate = pow((distToTarget - moveTolerance) / (flyStopFactor*moveTolerance - moveTolerance), 0.5f);
							myCharMove->Velocity *= deacelRate;
							if (debugInfo)
								UE_LOG(LogTemp, Warning, TEXT("[mutation] deacel %f"), deacelRate);
						}						
					}
				}
				else {					
					myCharMove->MovementMode = MOVE_Walking;
					flying = false;
					movingRes = myController->MoveToLocation(targetPos, moveTolerance, true, true, true, true, 0, true);
					if (movingRes == EPathFollowingRequestResult::RequestSuccessful) {
						UE_LOG(LogTemp, Warning, TEXT("[mutation] moving success"));
					}
					else {
						if (movingRes == EPathFollowingRequestResult::Failed) { UE_LOG(LogTemp, Warning, TEXT("[mutation] moving fail")); }
						else {
							UE_LOG(LogTemp, Warning, TEXT("[mutation] moving already at goal"));
						}
					}
				}
			}
		}
	}
	*/
	
	switch(mystate){
		case MutationStates::idle:
			idleTimer -= DeltaTime;
			if (idleTimer <= 0.0f) {
				ResetFightAnims();
			}
			//wait
			targetPos = myTarget->GetActorLocation();
			LookTo(targetPos);
			break;
		case MutationStates::patrol:
			Navigating(DeltaTime);
			break;
		case MutationStates::investigate:
			Navigating(DeltaTime);
			break;
		case MutationStates::fight:
			timeSeen = mytime;
			timeHeard = 0.0f;
			myCharMove->bOrientRotationToMovement = false;

			MutationFight(DeltaTime);							
			break;
		case MutationStates::escape:
			Navigating(DeltaTime);
			break;
		case MutationStates::pursuit:	
			Navigating(DeltaTime);
			break;
		case MutationStates::attacking:
			if ((Controller != NULL) && (advanceAtk != 0.0f))
			{
				// get forward vector
				moveDir = GetActorForwardVector();
				AddMovementInput(moveDir, advanceAtk);
			}
		break;
		case MutationStates::suffering:			
			moveDir = GetActorForwardVector();
			AddMovementInput(-moveDir, recoilPortion);
		break;
		case MutationStates::evading:
			if ((Controller != NULL) && (evadeValue != 0.0f))
			{
				moveDir = GetActorRightVector();
				AddMovementInput(moveDir, evadeValue);
			}
			targetPos = myTarget->GetActorLocation();
			LookTo(targetPos);
			break;
		case MutationStates::approach:
			if(Controller != NULL)
			{
				moveDir = GetActorForwardVector();
				AddMovementInput(moveDir, 1.0f);
			}
			targetPos = myTarget->GetActorLocation();
			LookTo(targetPos);
			//CheckRange();
			break;
		case MutationStates::kdFlight:
			if (Controller != NULL)
			{
				//set takeoff speed at 45 degrees, approximately sqrt(2)/2
				moveDir = 0.7f*FVector::UpVector - GetActorForwardVector();
				AddMovementInput(moveDir, recoilPortion);
			}
			break;
		case MutationStates::heightRoll:
			//rotate, despite the name, the maneuver is actually in pitch
			NewRotation = FRotator(heightRollSpeed*DeltaTime, 0.0f, 0.0f);
			QuatRotation = FQuat(NewRotation);
			AddActorLocalRotation(QuatRotation, false, 0, ETeleportType::None);
			//go to player's height
			moveDir = (-1)*FVector::UpVector-GetActorForwardVector();
			AddMovementInput(moveDir, 1.0f);
			
			//if finished roll, resume
			if (heightRollMid) {
				moveDir = FVector::VectorPlaneProject(targetPos - GetActorLocation(), FVector::UpVector);
				moveDir.Normalize();
				if (FMath::Acos(FVector::DotProduct(moveDir, GetActorForwardVector())) < heightRollTol) {
					ResetFightAnims();
				}
			}
			break;
		case MutationStates::grabbed:
			if(thrownByPlayer){
				GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, "[mutation] grabbed flying.");
				AddMovementInput(grabFlightDir, 1.0);
				someRotation = ((mytime - thrownTime) / grabRecoverTime)*ScanRotation;
				SetActorRotation(someRotation);
			}
			break;
		case MutationStates::dizzy:
			GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Orange, "[mutation] dizzy.");
			break;
	}
	
	/*
	if(targetSensedTime > 0 && mytime - targetSensedTime > blindPursuitTime){
		targetSensedTime = 0.0f;
		donePath = true;
		ArrivedAtGoal();		
	}
	*/
}

// Called to bind functionality to input
void AMutationChar::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AMutationChar::OnHearNoise(APawn* PawnInstigator, const FVector& Location, float Volume)
{
	//We don't want to hear ourselves
	if (myController && PawnInstigator != this)
	{
		myTarget = Cast<AMyPlayerCharacter>(PawnInstigator);
		if (myTarget != nullptr && mystate < MutationStates::suffering) {
			
			UE_LOG(LogTemp, Warning, TEXT("[Mutation] target heard."));

			if (mystate == MutationStates::escape) {
				myController->SetNeedHideSpot(true);
				waitEQS = true;
			}
			else {
				//Updates our target based on what we've heard.
				timeHeard = mytime;
				targetPos = PawnInstigator->GetActorLocation();
				targetInAir = myTarget->inAir;
				distToTarget = FVector::Distance(myLoc, targetPos);
				PawnSensingComp->SetActive(true);
				PawnSensingComp->SetSensingUpdatesEnabled(true);
				PawnSensingComp->bSeePawns = true;
				PawnSensingComp->bHearNoises = true;
				mystate = MutationStates::pursuit;
				currentScanParams = investigateParams;
				searchTimer = blindPursuitTime;
				StartTraverse();
				CancelEQS();
			}
			/*
			myController->SetGoal(targetPos);
			myController->SetTargetLocated(true);
			myController->SetGoalInAir(myTarget->inAir);
			
			
			flying = myTarget->inAir || inAir;
			
			distToTarget = FVector::Distance(GetActorLocation(), targetPos);
			inFightRange = distToTarget < fightRange;
			if (inFightRange){
				if (mystate != MutationStates::fight && mystate != MutationStates::approach && mystate != MutationStates::evading && mystate != MutationStates::attacking && mystate != MutationStates::heightRoll) {
					mystate = MutationStates::fight;
					DecideWhichSideFacesPlayer();
					myController->SetInFightRange(inFightRange);
					PawnSensingComp->SetActive(!inFightRange);
					PawnSensingComp->SetSensingUpdatesEnabled(!inFightRange);
					PawnSensingComp->bSeePawns = !inFightRange;
					PawnSensingComp->bHearNoises = !inFightRange;
				}
			}
			else { 
				StartTraverse(); 
			}
			*/
		}
	}
}

void AMutationChar::OnSeenTarget(APawn* PawnInstigator)
{
	//We don't want to hear ourselves
	if (myController && PawnInstigator != this)
	{
		myTarget = Cast<AMyPlayerCharacter>(PawnInstigator);
		if (myTarget != nullptr && mystate < MutationStates::suffering) {
			UE_LOG(LogTemp, Warning, TEXT("[Mutation] target seen!."));
			
			if (mystate == MutationStates::escape) {
				myController->SetNeedHideSpot(true);
				waitEQS = true;
			}
			else {
				//Updates our target based on what we've heard.
				targetVisible = true;
				timeSeen = mytime;
				targetPos = PawnInstigator->GetActorLocation();
				targetInAir = myTarget->inAir;
				distToTarget = FVector::Distance(myLoc, targetPos);
				PawnSensingComp->SetActive(false);
				PawnSensingComp->SetSensingUpdatesEnabled(false);
				PawnSensingComp->bSeePawns = false;
				PawnSensingComp->bHearNoises = false;
				mystate = MutationStates::pursuit;
				currentScanParams = investigateParams;
				searchTimer = blindPursuitTime;
				StartTraverse();
				CancelEQS();
				if(debugInfo)
					DrawDebugSphere(myWorld, targetPos, moveTolerance, 12, FColor::Green, true, 1.0f, 0, 5.0);
			}
			/*
			myController->SetTargetLocated(true);
			myController->SetTargetVisible(targetVisible);
			myController->SetGoal(targetPos);
			myController->SetGoalInAir(myTarget->inAir);
			
			flying = myTarget->inAir || inAir;
			
			distToTarget = FVector::Distance(GetActorLocation(), targetPos);
			inFightRange = distToTarget < fightRange;
			if (inFightRange) {
				if (mystate != MutationStates::fight && mystate != MutationStates::approach && mystate != MutationStates::evading && mystate != MutationStates::attacking && mystate != MutationStates::heightRoll) {
					mystate = MutationStates::fight;
					DecideWhichSideFacesPlayer();
					myController->SetInFightRange(inFightRange);
					PawnSensingComp->SetActive(!inFightRange);
					PawnSensingComp->SetSensingUpdatesEnabled(!inFightRange);
					PawnSensingComp->bSeePawns = !inFightRange;
					PawnSensingComp->bHearNoises = !inFightRange;
				}
			}
			else {
				StartTraverse();
			}
			*/
		}
	}
}
void AMutationChar::CheckDesperation(){
	if (life < desperateLifeLevel) {
		myController->SetDesperate(true);
		//myController->SetDonePath(true);
		ArrivedAtGoal();
	}
}
bool AMutationChar::CheckRange()
{
	distToTarget = FVector::Distance(GetActorLocation(), myTarget->GetActorLocation());
	inFightRange = distToTarget < fightRange;
	
	myController->SetInFightRange(inFightRange);
	PawnSensingComp->SetActive(!inFightRange);
	PawnSensingComp->SetSensingUpdatesEnabled(!inFightRange);
	PawnSensingComp->bSeePawns = !inFightRange;
	PawnSensingComp->bHearNoises = !inFightRange;
	
	myCharMove->StopActiveMovement();
	myController->StopMovement();
	myCharMove->Velocity *= 0;

	if (inFightRange) {
		if (mystate != MutationStates::fight) {
						
			DecideWhichSideFacesPlayer();
			mystate = MutationStates::fight;
			PawnSensingComp->SetActive(true);
			PawnSensingComp->SetSensingUpdatesEnabled(true);
			PawnSensingComp->bSeePawns = true;
			PawnSensingComp->bHearNoises = true;
			return false;
		}
		else {
			return true;
		}
	}
	else {
		reachedGoal = false;
		donePath = true;
		//myController->SetDonePath(true);
		NextPatrolPoint();
		ArrivedAtGoal();
		return false;
	}
}
void AMutationChar::LookTo(FVector LookPosition) {
	FRotator lookToRot = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), LookPosition);
	const FRotator lookToYaw(0, lookToRot.Yaw, 0);
	SetActorRotation(lookToYaw);
}

void AMutationChar::NextPatrolPoint()
{
	patrol_i = nextPatrol_i;
	if (patrolPoints.Num() >= 2) {
		if (patrol_in_order || patrolBackNforth) {
			nextPatrol_i += patrolDir;

			if (nextPatrol_i >= patrolPoints.Num()) {
				if (patrolBackNforth) {
					patrolDir *= -1;
					nextPatrol_i = patrolPoints.Num() - 2;
				}
				else {
					nextPatrol_i = 0;
				}
			}
			if (nextPatrol_i < 0) {
				if (patrolBackNforth) {
					patrolDir *= -1;
					nextPatrol_i = 1;
				}
			}
		}
		else {
			nextPatrol_i = FMath::RandRange(0, patrolPoints.Num() - 1);
			if (nextPatrol_i == patrol_i) {
				nextPatrol_i++;
			}
			if (nextPatrol_i >= patrolPoints.Num())
				nextPatrol_i = 0;
		}
	}
	//extract information from the TargetPoint MutationWaypoint
	UE_LOG(LogTemp, Warning, TEXT("Updated next patrol point to: %d"), nextPatrol_i);
}
void AMutationChar::Navigating(float DeltaTime){
	
	myLoc = GetActorLocation();
	forthVec = GetActorForwardVector();
	FVector targetDir = targetPos - myLoc;
	targetDir.Normalize();	

	switch (moveMode) {
	case MoveModes::waitOldHead:
		myCharMove->bOrientRotationToMovement = false;
		if (debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, TEXT("[Mutation] waiting in old heading"));
		if (mytime - startMoveTimer < currentScanParams.timeInOldHead && startMoveTimer != 0.0f) {}
		else {
			startMoveTimer = mytime;
			moveMode = MoveModes::scanning;
			angleToTurn2Target = FMath::RadiansToDegrees(FMath::Acos(targetDir.CosineAngle2D(forthVec)));
			UE_LOG(LogTemp, Warning, TEXT("Angle to face target %f"), angleToTurn2Target);
			
			//calculate what direction to turn to scan, it is the opposite from the direction he will need to go			
			FVector pseudoUp = FVector::CrossProduct(forthVec, targetDir);
			if (FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(pseudoUp, FVector::UpVector))) < 90)
				scanDir = -1;
			else
				scanDir = 1;

			//calculate speed to scan
			if(currentScanParams.timeToScan>0.0f)
				scanTurnSpeed = currentScanParams.angleToScan / currentScanParams.timeToScan;
		}
		break;
	case MoveModes::scanning:
		if (debugInfo) 
			GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, TEXT("[Mutation] scanning environment"));
		if (mytime - startMoveTimer < currentScanParams.timeToScan && startMoveTimer != 0.0f){
			myLoc = GetActorLocation();
			forthVec = GetActorForwardVector();
			DrawDebugLine(myWorld, myLoc, myLoc + forthVec * 100.0f, FColor::Black, true, 0.1f, 0, 5.0);
			DrawDebugLine(myWorld, myLoc, myLoc + targetDir * 100.0f, FColor::Green, true, 0.1f, 0, 5.0);
			//rotate scanning area
			ScanRotation = FRotator(0.0f, scanDir*scanTurnSpeed*DeltaTime, 0.0f);
			QuatScanRotation = FQuat(ScanRotation);
			AddActorLocalRotation(QuatScanRotation, false, 0, ETeleportType::None);
		}
		else{			
			startMoveTimer = mytime;
			moveMode = MoveModes::waitAfterScan;
		}
		break;
	case MoveModes::waitAfterScan:
		if (debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, TEXT("[Mutation] waiting after scan"));
		if (mytime - startMoveTimer < currentScanParams.timeInMidHead && startMoveTimer != 0.0f) {}
		else {
			startMoveTimer = mytime;
			moveMode = MoveModes::turn2NewHead;

			//calculate turn direction
			scanDir *= -1;
			//calculate turn speed
			if(currentScanParams.timeToLookNewHead > 0.0f)
				scanTurnSpeed = (angleToTurn2Target + currentScanParams.angleToScan) / currentScanParams.timeToLookNewHead;
		}
		break;
	case MoveModes::turn2NewHead:
		if (debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, TEXT("[Mutation] turning 2 new heading"));
		if (mytime - startMoveTimer < currentScanParams.timeToLookNewHead && startMoveTimer != 0.0f) {
			//rotate to new head
			ScanRotation = FRotator(0.0f, scanDir*scanTurnSpeed*DeltaTime, 0.0f);
			QuatScanRotation = FQuat(ScanRotation);
			AddActorLocalRotation(QuatScanRotation, false, 0, ETeleportType::None);
		}
		else {
			startMoveTimer = mytime;
			moveMode = MoveModes::waitInNewHead;
		}
		break;
	case MoveModes::waitInNewHead:
		if (debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Yellow, TEXT("[Mutation] waiting in new heading"));
		UE_LOG(LogTemp, Warning, TEXT("[Mutation] waiting in new heading"));
		if (mytime - startMoveTimer < currentScanParams.timeBeforeTraverse && startMoveTimer != 0.0f) {}
		else {
			//if (debugInfo)				
				//DrawDebugSphere(myWorld, targetPos, moveTolerance, 12, FColor::Green, true, 5.0f, 0, 5.0);
			
			startMoveTimer = 0.0f;

			if (mystate == MutationStates::patrol) {
				//go to next patrol point
				NextPatrolPoint();
				targetPos = patrolPoints[nextPatrol_i]->GetActorLocation();
				targetInAir = patrolPoints[nextPatrol_i]->airGoal;
				distToTarget = FVector::Distance(myLoc, targetPos);
				currentScanParams = patrolPoints[nextPatrol_i]->scanParams;
				StartTraverse();
			}
			if (mystate == MutationStates::pursuit) {
				//arrived at last known location, ask for a new hiding spot
				myController->SetNeedSeekSpot(true);
				UE_LOG(LogTemp, Warning, TEXT("[mutation] asked for a seek spot"));
				waitEQS = true;
				if (!canFly || !inAir) {
					if (searchTimer == 0.0f)
						searchTimer = blindPursuitTime;
				}
				else {
					searchTimer = blindPursuitTime;
				}
			}			
		}
		break;
	case MoveModes::traversing:
		//traverse AI
		//myLoc = GetActorLocation();
		distToTarget = FVector::Distance(myLoc, targetPos);
		if (distToTarget <= moveTolerance) {
			askedBestPath = 0;
			myController->StopMovement();
			moveMode = waitOldHead;
			if (mystate == MutationStates::patrol) {
				UE_LOG(LogTemp, Warning, TEXT("arrived at point: %d"), nextPatrol_i);
			}
			if (mystate == MutationStates::escape) {
				//arrived at last known hide spot, find another one
				myController->SetNeedHideSpot(true);
				UE_LOG(LogTemp, Warning, TEXT("[mutation] asked for a hidden spot"));
				waitEQS = true;
			}
		}
		else {
			//hitres = FHitResult(ForceInit);
			if (myWorld->LineTraceSingleByChannel(hitres, myLoc, targetPos, ECC_Camera, RayParams)) {
				if (debugInfo) {
					DrawDebugLine(myWorld, myLoc, targetPos, FColor::Green, true, 1.1f, 0, 5.0);
				}
				AMyPlayerCharacter * playerChar = Cast<AMyPlayerCharacter>(hitres.GetActor());
				if (playerChar != nullptr) {
					targetVisible = true;
					if (distToTarget < fightRange) {
						mystate = MutationStates::fight;
						threatened = true;
					}
					else {
						movingRes = myController->MoveToLocation(targetPos, 0.5f*moveTolerance, false, !canFly, !canFly, true, 0, true);
					}
				}
				else {
					//target no longer visible!
					targetVisible = false;
					//turn sensing back on
					PawnSensingComp->SetActive(true);
					PawnSensingComp->SetSensingUpdatesEnabled(true);
					PawnSensingComp->bSeePawns = true;
					PawnSensingComp->bHearNoises = true;

					//when obstructed by another mutation, we consider it is not obstructed
					AMutationChar * otherMut = Cast<AMutationChar>(hitres.GetActor());
					if (otherMut != nullptr) {
						//you forgot to set this mutation to be ignored in its trace channel!!!!
						GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString::Printf(TEXT("Mutation: %s is NOT being ignored on the path finding AI, fix its Mutations trace response!"), *hitres.Actor.Get()->GetName()));
					}
					else {
						//obstructed by static object most likely... it might be a dynamic object still
						if (canFly) {
							if (inAir) {
								if (askedBestPath <= 4) {
									//use EQS to find best path
									UE_LOG(LogTemp, Warning, TEXT("[mutation] getting a new unobstructed spot"));
									myController->SetGoal(targetPos);
									askedBestPath++;
									myController->SetNeedBestPath(true);
									waitEQS = true;
								}
								else {
									//random direction to clear obstruction
									UE_LOG(LogTemp, Warning, TEXT("[mutation] choosing random dir"));
									flying = true;
									myCharMove->MovementMode = MOVE_Flying;
									
									FVector randDest = targetPos + FMath::RandRange(0.0f, 500.0f)*FVector::RightVector + FMath::RandRange(0.0f, 500.0f)*FVector::ForwardVector;
									if (debugInfo) {
										DrawDebugLine(myWorld, randDest, randDest + 1000.0f*FVector::UpVector, FColor::Magenta, true, 1.1f, 0, 5.0);
									}
									movingRes = myController->MoveToLocation(randDest, 0.5f*moveTolerance, false, false, false, true, 0, true);
								}
							}
							else {
								movingRes = myController->MoveToLocation(targetPos, 0.5f*moveTolerance, false, true, true, true, 0, true);
							}
						}
						else {
							movingRes = myController->MoveToLocation(targetPos, 0.5f*moveTolerance, false, !canFly, !canFly, true, 0, true);
						}
					}
				}
			}
			else {
				//no obstruction at all
				movingRes = myController->MoveToLocation(targetPos, 0.5f*moveTolerance, false, !canFly, !canFly, true, 0, true);

				//target no longer visible!
				targetVisible = false;
				//turn sensing back on
				PawnSensingComp->SetActive(true);
				PawnSensingComp->SetSensingUpdatesEnabled(true);
				PawnSensingComp->bSeePawns = true;
				PawnSensingComp->bHearNoises = true;
			}
		}
		break;
	}
}
void AMutationChar::StartTraverse() {
	moveMode = MoveModes::traversing;	
	
	//reset speeds that were changed on the SpiralAttack
	myCharMove->MaxWalkSpeed = normalSpeed;
	myCharMove->MaxAcceleration = normalAcel;
	myCharMove->MaxFlySpeed = normalSpeed;
	advanceAtk = 0.0f;
	myCharMove->Velocity *= 0.0f;

	/*
	PawnSensingComp->SetActive(true);
	PawnSensingComp->SetSensingUpdatesEnabled(true);
	PawnSensingComp->bSeePawns = true;
	PawnSensingComp->bHearNoises = true;
	*/
	
	//oldTargetDist = -1.0f;
	startMoveTimer = mytime;
	UE_LOG(LogTemp, Warning, TEXT("[mutation %s] Starting traverse"),*GetName());
}
void AMutationChar::ArrivedAtGoal()
{
	myController->SetGoal(targetPos);
	if (myCharMove->IsMovingOnGround()) {
		flying = false;
		myController->SetAirborne(false);
		UE_LOG(LogTemp, Warning, TEXT("[navigation] it is grounded by char movement test"), nextPatrol_i);
	}

	//check if on the navigation mesh to find out if it is grounded
	if (flying) {
		movingRes = myController->MoveToLocation(targetPos, 5.0f, true, true, true, true, 0, true);
		if (movingRes == EPathFollowingRequestResult::RequestSuccessful || movingRes == EPathFollowingRequestResult::AlreadyAtGoal) {
			flying = false;
			myController->SetAirborne(false);
			myCharMove->MovementMode = MOVE_Walking;
			UE_LOG(LogTemp, Warning, TEXT("[navigation] it is grounded by AI test"), nextPatrol_i);
		}
		else{
			myController->SetAirborne(true);
		}
		myController->StopMovement();
	}		
	//free behaviour tree
	myController->SetDonePath(true);
}
void AMutationChar::NewGoal(bool Flying) {
	targetPos = myController->GetGoal();
	donePath = false;
	startMoveTimer = mytime;
	switch (mystate) {
		case MutationStates::patrol:
			moveMode = MoveModes::waitOldHead;
		break;
		case MutationStates::investigate:
			moveMode = MoveModes::waitOldHead;
			currentScanParams = investigateParams;
			break;
		case MutationStates::pursuit:
			currentScanParams = investigateParams;
			moveMode = MoveModes::waitOldHead;
			break;
		case MutationStates::escape:
			StartTraverse();
			break;
	}	

	flying = Flying;	
	if (flying) {
		myCharMove->MovementMode = MOVE_Flying;
	}
	else {
		myCharMove->MovementMode = MOVE_Walking;
	}	
}

void AMutationChar::MutationFight(float DeltaTime) {
	UE_LOG(LogTemp, Warning, TEXT("[Mutation %s fighting"), *GetName());

	//reset speeds that were changed on the SpiralAttack
	myCharMove->MaxWalkSpeed = normalSpeed;
	myCharMove->MaxAcceleration = normalAcel;
	myCharMove->MaxFlySpeed = normalSpeed;
	advanceAtk = 0.0f;
	myCharMove->Velocity *= 0.0f;

	//find the angle between player and mutation
	targetPos = myTarget->GetActorLocation();
	FVector mypos = GetActorLocation();
	FVector targetDir = targetPos - mypos;
	targetDir.Normalize();
	float facingAngle = FMath::RadiansToDegrees(FMath::Acos(targetDir.CosineAngle2D(GetActorForwardVector())));
	if (facingAngle < faceTolerance) {		
		if (distToTarget < strikeDistance) {
			//height roll
			if(mypos.Z > targetPos.Z + rollTolHeight){
				CombatAction(3, DeltaTime);
			}
			else{
				CombatAction(0, DeltaTime);
			}
		}
		else {
			if (distToTarget < 2 * strikeDistance) {
				CombatAction(1, DeltaTime);
			}
			else {
				CombatAction(2, DeltaTime);
			}
		}			
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Turning to face player"));
		//rotate to face the player
		NewRotation = FRotator(0.0f, rotateToFaceTargetDir*faceTargetRotSpeed*DeltaTime, 0.0f);
		QuatRotation = FQuat(NewRotation);
		AddActorLocalRotation(QuatRotation, false, 0, ETeleportType::None);
	}	
}
void AMutationChar::CombatAction(int near_i, float DeltaTime){
	//UE_LOG(LogTemp, Warning, TEXT("[combat action] Mutation %s is %d"), *GetName(), near_i);
	//to stop drifting when fighting
	myCharMove->StopActiveMovement();
	myController->StopMovement();
	myCharMove->Velocity *= 0;
	if (FMath::RandRange(0.0f, 1.0f) <= aggressivity && myTarget->mystate != myTarget->PlayerStates::kdRise) {
		
		//attack
		switch (near_i) {
			case 0://close combat
				attackConnected = false;
				//look in player's direction right before first attack
				//targetPos = myTarget->GetActorLocation();
				//LookTo(targetPos);
				MeleeAttack();
				break;
			case 1://middle range attacks
				if (FMath::RandRange(0.0f, 1.0f) <= aggressivity || threatened)
					SpiralAttack();
				else
					DashSideways();
				break;
			case 2://long distance behaviour
				if (FMath::RandRange(0.0f, 1.0f) <= aggressivity || threatened)
					Approach();
				else
					DashSideways();
				break;
			case 3:
				heightRollMid = false;
				GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::ClearHeightRoll, 0.3f, false);
				//do a height Roll
				mystate = MutationStates::heightRoll;
				myCharMove->bOrientRotationToMovement = false;
				break;
			default:
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Mutation] combatAction ID not recognized!"));
				break;
		}
		threatened = false;
	}
	else {
		//wait
		idleTimer = FMath::RandRange(minIdleTime, maxIdleTime / (3 - near_i));
		mystate = MutationStates::idle;				
	}	
}

void AMutationChar::StartLethal() {
	advanceAtk = atkWalker->advanceAtkValue;
	knockingDown = atkWalker->knockDown;
	if(mainWeaponComp)
		Cast<UPrimitiveComponent>(mainWeaponComp)->SetGenerateOverlapEvents(true);

	float time2NotLethal = atkWalker->lethalTime*(1 - atkWalker->telegraphPortion)*(atkWalker->myAnim->SequenceLength / atkWalker->speed);
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::StopLethal, time2NotLethal, false);
}
void AMutationChar::StopLethal() {
	//update targetPos, so the mutation faces in the direction the player evaded
	targetPos = myTarget->GetActorLocation();
	//reset speeds that were changed on the SpiralAttack
	myCharMove->MaxWalkSpeed = normalSpeed;
	myCharMove->MaxAcceleration = normalAcel;
	myCharMove->MaxFlySpeed = normalSpeed;
	advanceAtk = 0.0f;
	myCharMove->Velocity *= 0.0f;

	//reset orientation
	//find forward projection on floor
	FVector projForth = FVector::VectorPlaneProject(GetActorForwardVector(), FVector::UpVector);
	FVector forthPos = GetActorLocation() + projForth;
	FRotator lookToRot = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), forthPos);
	const FRotator lookToYaw(0, lookToRot.Yaw, 0);	
	SetActorRotation(lookToYaw);

	if (mainWeaponComp)
		Cast<UPrimitiveComponent>(mainWeaponComp)->SetGenerateOverlapEvents(false);
	float time4NextHit = (1 - (atkWalker->time2lethal + atkWalker->lethalTime))*(1-atkWalker->telegraphPortion)*(atkWalker->myAnim->SequenceLength / atkWalker->speed) + atkWalker->coolDown;
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::NextComboHit, time4NextHit, false);
}

void AMutationChar::ResetFightAnims() {
	UE_LOG(LogTemp, Warning, TEXT("[mutation] finished attacking"));
	atkWalker = &attackList[0];
	heightRollMid = false;
		
	//stop drifting
	myCharMove->StopActiveMovement();
	myController->StopMovement();
	myCharMove->Velocity *= 0;
	
	if (myMesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint) {
		myMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		myAnimBP = Cast<UMutationAnimComm>(myMesh->GetAnimInstance());
	}

	if (debugInfo)
		GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Blue, TEXT("[Mutation] following the anim blueprint again!"));
	
	myLoc = GetActorLocation();
	distToTarget = FVector::Distance(myLoc, myTarget->GetActorLocation());
	if (distToTarget > fightRange) {
		mystate = MutationStates::pursuit;
		StartTraverse();
	}
	else {
		DecideWhichSideFacesPlayer();
		mystate = MutationStates::fight;
	}
	//CheckRange();
}
void AMutationChar::Investigate() {
	mystate = MutationStates::investigate;
	//myController->SetReachedGoal(true);
	myController->SetTargetVisible(false);
	ArrivedAtGoal();
}

void AMutationChar::MeleeAttack() {
	UE_LOG(LogTemp, Warning, TEXT("mutation %s in meleeAttack"), *GetName());
	threatened = false;
	spiralAtk = false;
	mystate = MutationStates::attacking;
	//myController->StopBT();
	myMesh->Stop();

	//look in player's direction right before first attack
	targetPos = myTarget->GetActorLocation();
	LookTo(targetPos);

	myCharMove->StopActiveMovement();
	myController->StopMovement();
	myCharMove->Velocity *= 0;	

	myMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	myMesh->PlayAnimation(atkWalker->myAnim, false);
	UGameplayStatics::PlaySoundAtLocation(myWorld, meleeAtkSFX, GetActorLocation(), SFXvolume);

	float time2Lethal = atkWalker->time2lethal*(atkWalker->myAnim->SequenceLength / atkWalker->speed);
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::StartLethal, time2Lethal, false);
}
void AMutationChar::SpiralAttack() {
	UE_LOG(LogTemp, Warning, TEXT("mutation %s in spiral attack"), *GetName());
	threatened = false;
	spiralAtk = true;
	myCharMove->MaxWalkSpeed = spiralSpeed;	
	myCharMove->MaxAcceleration = spiralAcel;
	myCharMove->MaxFlySpeed = spiralSpeed;
	
	mystate = MutationStates::attacking;
	//myController->StopBT();
	myMesh->Stop();
	targetPos = myTarget->GetActorLocation();
	//LookTo(targetPos);
	FRotator lookToRot = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), targetPos);
	SetActorRotation(lookToRot);

	myCharMove->StopActiveMovement();
	myController->StopMovement();
	myCharMove->Velocity *= 0;
	atkWalker = &spiralAtkNode;
	advanceAtk = atkWalker->telegraphAdvance;
	
	myMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	myMesh->PlayAnimation(spiralAtkNode.myAnim, false);
	UGameplayStatics::PlaySoundAtLocation(myWorld, spiralAtkSFX, GetActorLocation(), SFXvolume);
	
	float time2Lethal = atkWalker->telegraphPortion*(atkWalker->myAnim->SequenceLength / atkWalker->speed);
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::StartLethal, time2Lethal, false);
}
void AMutationChar::Approach() {
	UE_LOG(LogTemp, Warning, TEXT("mutation %s getting closer"), *GetName());
	mystate = MutationStates::approach;
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::ResetFightAnims, dashTime, false);
}
void AMutationChar::DashSideways(){
	UE_LOG(LogTemp, Warning, TEXT("mutation %s dashing sideways"), *GetName());
	mystate = MutationStates::evading;
	if (FMath::RandRange(0.0f, 1.0f) < 0.5)
		evadeValue = 1.0f;
	else
		evadeValue = -1.0f;

	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::ResetFightAnims, dashTime, false);
}
void AMutationChar::NextComboHit() {
	if (distToTarget < strikeDistance) {
		//Decide if following up or just restarting attacks
		//if (FMath::RandRange(0.0f, 1.0f) < aggressivity) {
		if(!attackConnected){
			ResetFightAnims();
		}
		else {
			//forcing to go to the left because we lack the animations for the right
			if (FMath::RandRange(0.0f, 1.0f) < -1.0f) {
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

void AMutationChar::CancelAttack() {
	myWorld->GetTimerManager().ClearTimer(timerHandle);
	myCharMove->MaxWalkSpeed = normalSpeed;
	myCharMove->MaxAcceleration = normalAcel;
	myController->StopBT();
	
	advanceAtk = 0.0f;
	if (mainWeaponComp)
		Cast<UPrimitiveComponent>(mainWeaponComp)->SetGenerateOverlapEvents(false);

	//reset anims
	myMesh->Stop();
	myMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	myAnimBP = Cast<UMutationAnimComm>(myMesh->GetAnimInstance());
}

void AMutationChar::MyDamage(float DamagePwr, FVector AlgozPos, bool KD, float RecoilForce, float DmgTime, FVector hitPoint, FVector hitNormal) {
	if (debugInfo) {
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, "[Mutation] damaged: "+GetName());
	}
	UE_LOG(LogTemp, Warning, TEXT("mutation %s in damage"), *GetName());

	threatened = true;
	mystate = MutationStates::suffering;
	damageTime = DmgTime;

	//stop the traversing
	donePath = true;
	
	CancelAttack();
	CancelEQS();
	myCharMove->StopActiveMovement();
	myController->StopMovement();
	myCharMove->Velocity *= 0;
	myCharMove->bOrientRotationToMovement = false;
	
	//look to your algoz
	LookTo(AlgozPos);
	
	if (life - DamagePwr < desperateLifeLevel && life >= desperateLifeLevel) {
		//myController->SetDesperate(true);
		becomeDesperate = true;
	}

	FTransform hitTransform;
	hitTransform.SetLocation(hitPoint);
	//DrawDebugLine(myWorld, hitPoint, hitPoint+500.0f*FVector::UpVector, FColor::Red, true, 1.0f, 0, 5.0);
	hitTransform.SetRotation(hitNormal.ToOrientationQuat());
		
	UGameplayStatics::SpawnEmitterAtLocation(myWorld, damagehitVFX, hitTransform, true, EPSCPoolMethod::AutoRelease);

	life -= DamagePwr;
	if (life <= 0) {
		Death();
	}
	else {
		recoilPortion = RecoilForce;
		//play damage animation
		if (KD) {
			flying = false;
			myCharMove->MovementMode = MOVE_Flying;

			//play damage sound

			myAnimBP->damage = 10;

			//hit pause, or, in this case, time dilation
			UGameplayStatics::SetGlobalTimeDilation(myWorld, 0.1f);
			UGameplayStatics::PlaySoundAtLocation(myWorld, knockDownSFX, GetActorLocation(), SFXvolume);
			//start stabilize timer
			GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::DelayedStartKDtakeOff, hitPauseDuration, false);
		}
		else {

			//play damage sound	
			if (FMath::RandRange(0, 10) < 5) {
				myAnimBP->damage = 1;
				UGameplayStatics::PlaySoundAtLocation(myWorld, damage1SFX, GetActorLocation(), SFXvolume);
			}
			else {
				myAnimBP->damage = 2;
				UGameplayStatics::PlaySoundAtLocation(myWorld, damage2SFX, GetActorLocation(), SFXvolume);
			}

			//hit pause, or, in this case, time dilation
			UGameplayStatics::SetGlobalTimeDilation(myWorld, 0.1f);
			//start stabilize timer
			GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::DelayedStabilize, hitPauseDuration, false);
		}
	}
}
void AMutationChar::Death() {
	myWorld->GetTimerManager().ClearTimer(timerHandle);
	
	if (debugInfo) {
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, "[Mutation %s] is dead" + GetName());		
	}
	
	//check if player was locked to me and disengage targetlock if so
	myTarget->MutationDied(mutation_i);

	myGameState->RemoveMutation(mutation_i);

	AActor::Destroy();
}
void AMutationChar::DelayedStartKDtakeOff() {
	UGameplayStatics::SetGlobalTimeDilation(myWorld, 1.0f);
	mystate = MutationStates::kdFlight;
	kdrecovering = true;
	
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::DelayedStartKDFlight, knockDownTakeOffTime, false);
}
void AMutationChar::DelayedStartKDFlight() {
	//here the mutation starts falling	
	myCharMove->MovementMode = MOVE_Walking;
	kdrecovering = false;
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::DelayedStartKDFall, 0.5f, false);
}
void AMutationChar::DelayedStartKDFall() {
	recoilPortion = 0.0f;
}
void AMutationChar::DelayedStabilize() {
	UGameplayStatics::SetGlobalTimeDilation(myWorld, 1.0f);
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::Stabilize, damageTime, false);
}
void AMutationChar::DelayedKDRise() {
	myAnimBP->damage = 12;
	mystate = MutationStates::kdRise;
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::Stabilize, KDriseTime, false);
}
void AMutationChar::Stabilize() {
	UE_LOG(LogTemp, Warning, TEXT("[mutation] recovered from damage"));

	//reset speeds that were changed on the SpiralAttack
	myCharMove->MaxWalkSpeed = normalSpeed;
	myCharMove->MaxAcceleration = normalAcel;
	myCharMove->MaxFlySpeed = normalSpeed;
	advanceAtk = 0.0f;
	myCharMove->Velocity *= 0.0f;

	myController->RestartBT();

	myAnimBP->damage = 0;
	recoilPortion = 0.0f;

	if (becomeDesperate) {
		becomeDesperate = false;
		//myController->SetDonePath(true);
		//ArrivedAtGoal();
		mystate = MutationStates::escape;
		myController->SetNeedHideSpot(true);
		waitEQS = true;
	}
	else{
		targetPos = myTarget->GetActorLocation();
		if (mystate != MutationStates::escape) {
			mystate = MutationStates::fight;
			DecideWhichSideFacesPlayer();
		}
	}
}

void AMutationChar::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && (OtherActor != this) && OtherComp)
	{
		if (debugInfo) {
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::Printf(TEXT("[OverlapBegin] my name: %s hitObj %s compName %s className %s"), *GetName(), *OtherActor->GetName(), *OtherComp->GetName(), *OtherActor->GetClass()->GetName()));
		}
		
		//the safe cast crashes the game instead of returning nullptr for the rippleCollider, so we check before it:
		if (OtherComp->GetName() != "sword" && OtherComp->GetName() != "hook" && OtherComp->GetName() != "grabCollider")
			return;

		myTarget = Cast<AMyPlayerCharacter>(OtherActor);		
		if (myTarget != nullptr) {		
			if (OtherComp->GetName() == myTarget->swordComp->GetName() || (OtherComp->GetName() == myTarget->hookComp->GetName() && !myTarget->waiting4HookCol)) {
				if (mystate != MutationStates::kdFlight && mystate != MutationStates::grabbed) {
					//crazy engine, the hitPoint is not where the hit happened!!! The overlap does not populate the FHitResult struct
					//MyDamage(myTarget->attackPower, myTarget->GetActorLocation(), myTarget->knockingDown, myTarget->attackPush, myTarget->atkPushTime, SweepResult.ImpactPoint, SweepResult.ImpactNormal);
					MyDamage(myTarget->attackPower, myTarget->GetActorLocation(), myTarget->knockingDown, myTarget->attackPush, myTarget->atkPushTime, GetActorLocation(), FVector::UpVector);
				}
			}
			else {
				//check if grab
				if (OtherComp->GetName() == myTarget->grabComp->GetName()) {
					GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, "[Mutation grabbed] my name: " + GetName() + " hit obj: " + *OtherActor->GetName());
					if (!myTarget->mutationGrabbed) {
						thrownByPlayer = false;
						CancelAttack();
						mystate = MutationStates::grabbed;

						myTarget->grabbedMutation = this;
						UGameplayStatics::PlaySoundAtLocation(myWorld, begrabbedSFX, GetActorLocation(), SFXvolume);
						myTarget->GrabSuccess();

						grabbingSocketName = OtherComp->GetAttachSocketName();

						//change parent to player hand socket
						myMesh->AttachToComponent(OtherComp, FAttachmentTransformRules::KeepRelativeTransform, grabbingSocketName);

						//turn on ragdoll stuff
						inAir = true;
						myAnimBP->inAir = inAir;
						SetRagdoll(true);

						//reset speeds that were changed on the SpiralAttack
						myCharMove->MaxWalkSpeed = normalSpeed;
						myCharMove->MaxAcceleration = normalAcel;
						myCharMove->MaxFlySpeed = normalSpeed;
						advanceAtk = 0.0f;
						myCharMove->Velocity *= 0.0f;

						if (mainWeaponComp)
							Cast<UPrimitiveComponent>(mainWeaponComp)->SetGenerateOverlapEvents(false);
					}
					else {
						//send a grab fail
						myTarget->GrabFail();
					}
				}
				else {
					//some other non intended collider:
					GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, "[Mutation ODD overlapped] my name: " + GetName() + " hit obj: " + *OtherActor->GetName());
				}
			}
		}		
	}
}

void AMutationChar::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && (OtherActor != this) && OtherComp)
	{
		if (debugInfo) {
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Black, FString::Printf(TEXT("[OverlapEnd] my name: %s hitObj %s compName %s otherIndex %d"), *GetName(), *OtherActor->GetName(), *OtherComp->GetName(), OtherBodyIndex));			
		}
	}
}
void AMutationChar::Grappled() {
	thrownByPlayer = false;
	CancelAttack();
	mystate = MutationStates::grabbed;
}
void AMutationChar::DelayedFromGrabRecover(){
	FVector projForth = FVector::VectorPlaneProject(GetActorForwardVector(), FVector::UpVector);
	FVector forthPos = GetActorLocation() + projForth;
	ScanRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), forthPos);

	thrownTime = mytime;
	inAir = true;
	
	//enable collisions so mutations do not get throw through objects
	myCapsuleComp->SetGenerateOverlapEvents(true);
	//myCapsuleComp->BodyInstance.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics, false);
	myCapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	//myCapsuleComp->SetGenerateOverlapEvents(true);

	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::FromGrabRecover, grabRecoverTime, false);
}
void AMutationChar::FromGrabRecover() {
	SetActorRotation(ScanRotation);
	//turn off ragdoll stuff
	SetRagdoll(false);

	myCharMove->MaxFlySpeed = normalSpeed;
	myCharMove->MaxWalkSpeed = normalSpeed;
	myCharMove->MaxAcceleration = normalAcel;

	ResetFightAnims();
	//myCapsuleComp->SetGenerateOverlapEvents(true);
	//release it to fight
	mystate = MutationStates::fight;
	threatened = true;
	DecideWhichSideFacesPlayer();
}
void AMutationChar::OutOfAction() {
	CancelAttack();
	myController->StopBT();
	myCharMove->StopActiveMovement();
	myController->StopMovement();
	myCharMove->Velocity *= 0;
	mystate = MutationStates::dizzy;
}
void AMutationChar::FromGrappleRecover(float DizzyTime) {
	OutOfAction();
	
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMutationChar::Stabilize, DizzyTime, false);
}
void AMutationChar::SetRagdoll(bool Activation) {
	//turn off collisions on original collider
	myCapsuleComp->SetGenerateOverlapEvents(!Activation);
	//turn off collisions on damageDetector
	//damageDetector->SetGenerateOverlapEvents(!Activation);
	
	//myMesh->SetAllBodiesBelowSimulatePhysics(skeletonRootName, Activation, false);
	if (Activation) {
		//myCapsuleComp->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision, false);		
		myCapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		//myMesh->SetAllBodiesBelowPhysicsBlendWeight(skeletonRootName, 1.0f, false, false);
		UE_LOG(LogTemp, Warning, TEXT("[mutation] without collision"));
	}
	else{
		//myCapsuleComp->BodyInstance.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics, false);
		myCapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		UE_LOG(LogTemp, Warning, TEXT("[mutation grab] collision enabled"));
		//myMesh->SetAllBodiesBelowPhysicsBlendWeight(skeletonRootName, 0.0f, false, false);
		myController->RestartBT();
	}
}
void AMutationChar::StartPatrol() {
	if (patrolPoints.Num() > 0) {
		targetPos = patrolPoints[0]->GetActorLocation();
		nextPatrol_i = 0;
	}
	myLoc = GetActorLocation();
	currentScanParams = investigateParams;
}
void AMutationChar::GrabThrow(FVector GrabThrowDir, float GrabThrowSpeed) {
	CancelAttack();
	myCharMove->MovementMode = MOVE_Flying;
	myCharMove->StopActiveMovement();

	myCharMove->MaxFlySpeed = GrabThrowSpeed;
	myCharMove->MaxAcceleration = spiralAcel;

	thrownByPlayer = true;
	grabFlightDir = GrabThrowDir;
	//start delayed recover
	DelayedFromGrabRecover();
}
void AMutationChar::ClearHeightRoll() {
	heightRollMid = true;
}
void AMutationChar::CancelEQS() {
	myController->SetNeedHideSpot(false);
	myController->SetNeedSeekSpot(false);
	myController->SetNeedBestPath(false);
	
	myController->RestartBT();
	askedBestPath = 0;
	waitEQS = false;
}
void AMutationChar::DecideWhichSideFacesPlayer() {
	if (myTarget != nullptr) {
		//calculating what direction to rotate to face player
		FVector targetDir = myTarget->GetActorLocation() - GetActorLocation();
		targetDir.Normalize();
		FVector pseudoUp = FVector::CrossProduct(GetActorForwardVector(), targetDir);
		if (FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(pseudoUp, FVector::UpVector))) < 90)
			rotateToFaceTargetDir = 1;
		else
			rotateToFaceTargetDir = -1;
	}
}
void AMutationChar::FellOutOfWorld(const class UDamageType& dmgType)
{
	UE_LOG(LogTemp, Warning, TEXT("mutation fell from the world"));
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Black, "[mutation] " + GetName() + " fell from world");

	Death();
}
