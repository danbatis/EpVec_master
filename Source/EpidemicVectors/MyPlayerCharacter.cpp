// Fill out your copyright notice in the Description page of Project Settings.

#include "MyPlayerCharacter.h"

// Sets default values
AMyPlayerCharacter::AMyPlayerCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	/*
	GetCapsuleComponent()->SetSimulatePhysics(true);
	GetCapsuleComponent()->SetNotifyRigidBodyCollision(true);
	GetCapsuleComponent()->BodyInstance.SetCollisionProfileName("BlockAllDynamic");
	*/

	//collisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("collisionCapsule"));	
	//collisionCapsule->SetupAttachment(GetCapsuleComponent());

	//collisionCapsule->OnComponentHit.AddDynamic(this, &AMyPlayerCharacter::OnCompHit);
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &AMyPlayerCharacter::OnBeginOverlap);
	GetCapsuleComponent()->OnComponentEndOverlap.AddDynamic(this, &AMyPlayerCharacter::OnEndOverlap);
	//collisionCapsule->OnComponentBeginOverlap.AddDynamic(this, &AMyPlayerCharacter::OnBeginOverlap);
	//collisionCapsule->OnComponentEndOverlap.AddDynamic(this, &AMyPlayerCharacter::OnEndOverlap);

	//collisionCapsule->AttachToComponent(RootComponent);

	// Set as root component
	RootComponent = GetCapsuleComponent();

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;
	//for camera sensitivity
	TurnRate = 1.0f;
	LookUpRate = 1.0f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	//GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	//GetCharacterMovement()->JumpZVelocity = 600.f;
	
	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = CameraFreeArmLength; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	//the noise emitter for our AI
	PawnNoiseEmitterComp = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("PawnNoiseEmitterComp"));
}

// Called when the game starts or when spawned
void AMyPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	mystate = idle;
	//getting my basic animation blueprint communication class
	myMesh = GetMesh();
	myAnimBP = Cast<UAnimComm>(myMesh->GetAnimInstance());
	//find the 3 child components, the sword, the hook and the grab collider
	if (myMesh->GetChildComponent(0) && myMesh->GetChildComponent(1) && myMesh->GetChildComponent(2)) {
		if (myMesh->GetChildComponent(0)->GetName() == "sword") {
			swordComp = myMesh->GetChildComponent(0);
		}
		else {
			if (myMesh->GetChildComponent(0)->GetName() == "hook")
				hookComp = myMesh->GetChildComponent(0);
			else
				grabComp = myMesh->GetChildComponent(0);			
		}
		if (myMesh->GetChildComponent(1)->GetName() == "sword") {
			swordComp = myMesh->GetChildComponent(1);
		}
		else {
			if (myMesh->GetChildComponent(1)->GetName() == "hook")
				hookComp = myMesh->GetChildComponent(1);
			else
				grabComp = myMesh->GetChildComponent(1);
		}
		if (myMesh->GetChildComponent(2)->GetName() == "sword") {
			swordComp = myMesh->GetChildComponent(2);
		}
		else {
			if (myMesh->GetChildComponent(2)->GetName() == "hook")
				hookComp = myMesh->GetChildComponent(2);
			else
				grabComp = myMesh->GetChildComponent(2);
		}

		Cast<UPrimitiveComponent>(swordComp)->SetGenerateOverlapEvents(false);
		Cast<UPrimitiveComponent>(hookComp)->SetGenerateOverlapEvents(false);
		Cast<UPrimitiveComponent>(grabComp)->SetGenerateOverlapEvents(false);
		
		if(debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Blue, FString::Printf(TEXT("[player childs] sword: %s hook: %s grabCollider: %s"), *swordComp->GetName(), *hookComp->GetName(), *grabComp->GetName()));
	}
	else {
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::Printf(TEXT("[player childs] missing children!!!")));
	}
	hookRelPos = hookComp->GetRelativeTransform().GetLocation();
	hookRelRot = hookComp->GetRelativeTransform().GetRotation();

	//inverting joystick
	invertJoystickY ? verticalJoyDir = 1 : verticalJoyDir = -1;

	world = GetWorld();
	player = UGameplayStatics::GetPlayerController(world, 0);//crashes if in the constructor	
	
	Cast<UPrimitiveComponent>(hookComp)->OnComponentBeginOverlap.AddDynamic(this, &AMyPlayerCharacter::HookOverlap);

	/*
	//getting keys according to what is configured in the input module
	for(int i=0; i< player->PlayerInput->ActionMappings.Num();++i){
		if (player->PlayerInput->ActionMappings[i].ActionName.GetPlainNameString() == "Attack1")
			atk1Key = player->PlayerInput->ActionMappings[i].Key;
		if (player->PlayerInput->ActionMappings[i].ActionName.GetPlainNameString() == "Attack2")
			atk2Key = player->PlayerInput->ActionMappings[i].Key;
		if (player->PlayerInput->ActionMappings[i].ActionName.GetPlainNameString() == "Jump")
			jumpKey = player->PlayerInput->ActionMappings[i].Key;
		if (player->PlayerInput->ActionMappings[i].ActionName.GetPlainNameString() == "Hook")
			hookKey = player->PlayerInput->ActionMappings[i].Key;
		if (player->PlayerInput->ActionMappings[i].ActionName.GetPlainNameString() == "Shield")
			shieldKey = player->PlayerInput->ActionMappings[i].Key;
		if (player->PlayerInput->ActionMappings[i].ActionName.GetPlainNameString() == "Dash")
			dashKey = player->PlayerInput->ActionMappings[i].Key;
		//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::White, "keyName: "+ player->PlayerInput->ActionMappings[i].ActionName.GetPlainNameString());
	}
	for (int i = 0; i < player->PlayerInput->AxisMappings.Num(); ++i) {
		if (player->PlayerInput->AxisMappings[i].AxisName.GetPlainNameString() == "MoveForward") {
			verticalIn = player->PlayerInput->AxisMappings[i].Key;
			UE_LOG(LogTemp, Warning, TEXT("Set MoveForward key"));
		}
		if (player->PlayerInput->AxisMappings[i].AxisName.GetPlainNameString() == "MoveRight")
			horizontalIn = player->PlayerInput->AxisMappings[i].Key;
		if (player->PlayerInput->AxisMappings[i].AxisName.GetPlainNameString() == "Turn")
			horizontalCamIn = player->PlayerInput->AxisMappings[i].Key;
		if (player->PlayerInput->AxisMappings[i].AxisName.GetPlainNameString() == "LookUp")
			verticalCamIn = player->PlayerInput->AxisMappings[i].Key;
		UE_LOG(LogTemp, Warning, TEXT("axisName: %s"), *player->PlayerInput->AxisMappings[i].AxisName.GetPlainNameString());
	}
	*/

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
	for (int i = 0; i < attackAirList.Num(); ++i) {
		//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::White, "animName: " + attackTreeL[i].myAnim->GetFName().GetPlainNameString());
		if (attackAirList[i].leftNode != 0) {
			attackAirList[i].left = &attackAirList[attackAirList[i].leftNode];
			//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::White, "updated left leg");
		}
		if (attackAirList[i].rightNode != 0) {
			attackAirList[i].right = &attackAirList[attackAirList[i].rightNode];
			//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::White, "updated right leg");
		}
	}

	/*
	//use walker to show links work
	//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::White, "leftAttack chain:");
	UE_LOG(LogTemp, Warning, TEXT("walking in atk node always to the left"));
	atkWalker = &attackList[0];
	while(atkWalker->left != nullptr){
		atkWalker = atkWalker->left;
		//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::White, "animName: " + atkWalker->myAnim->GetFName().GetPlainNameString());
		UE_LOG(LogTemp, Warning, TEXT("leftAttack chain : %s"), *atkWalker->myAnim->GetFName().GetPlainNameString());
	}
	*/

	player->GetViewportSize(width, height);
	myCharMove = GetCharacterMovement();

	ResetSpeeds();
	
	//myCharMove->bOrientRotationToMovement = !lookInCamDir;
	
	//to signal there is no target selected or locked
	target_i = -1;
	RayParams.AddIgnoredActor(this);

	myGameState = Cast<AVectorsGameStateBase>(world->GetGameState());
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, "Finished beginPlay on player.");

	/*
	static ConstructorHelpers::FObjectFinder WaveRippleOb(TEXT("Blueprint'/EVteam/Blueprints/player/groundPunchRip_BP.groundPunchRip_BP'"));
	if (WaveRippleOb.Object != NULL)
	{
		WaveRippleBP = (UClass*)WaveRippleOb.Object->GeneratedClass;
	}
	*/
}

// Called every frame
void AMyPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (debugInfo)
		GEngine->AddOnScreenDebugMessage(-1, 0.01f, FColor::Blue, FString::Printf(TEXT("[playerState: %d"), mystate));
	
	if(!simChainCable) {
		if (!_previousCableWorldLocation.IsZero())
		{
			hookChain->ApplyWorldOffset(hookChain->GetComponentLocation() - _previousCableWorldLocation, false);
		}

		_previousCableWorldLocation = hookChain->GetComponentLocation();
	}

	mytime += DeltaTime;
	if(interactionLevel >= 1){
		vertIn = player->GetInputAnalogKeyState(vertical_Up) + (-1)*player->GetInputAnalogKeyState(vertical_Down) + player->GetInputAnalogKeyState(vertical_j);
		horIn = player->GetInputAnalogKeyState(horizontal_R) + (-1)*player->GetInputAnalogKeyState(horizontal_L) + player->GetInputAnalogKeyState(horizontal_j);

		if (vertIn <= 0.0f)
			mainThrustVFX->Deactivate();
	}
	else {
		vertIn = 0.0f; horIn = 0.0f;
	}

	altVertIn = (-1)*player->GetInputAnalogKeyState(verticalCam) + player->GetInputAnalogKeyState(vertical_jCam);
	altHorIn = player->GetInputAnalogKeyState(horizontalCam) + player->GetInputAnalogKeyState(horizontal_jCam);

	//because it is possible to change the resolution on the editor
	player->GetViewportSize(width, height);
	if (nearCamStart > 0.0f && mytime <= nearCamStart + reorientTime) {
		float nearCamGain = (mytime - nearCamStart) / reorientTime;
		cameraArmLength += (cameraArmLengthTarget - cameraArmLength)*nearCamGain;
		CameraBoom->TargetArmLength = cameraArmLength;
	}
	else {
		nearCamStart = 0.0f;
	}

	if (mystate < grabbing) {
		inAir = GetMovementComponent()->IsFalling() || flying;
	}
	if(!inAir) {
		//if (debugInfo)
		//	GEngine->AddOnScreenDebugMessage(-1, 0.1f, FColor::Blue, TEXT("falling..."));
		if(!grounded && mystate == idle){
			landing = true;
			CancelAttack();
			targetScreenPos.X = -1.0f;
			targetScreenPos.Y = -1.0f;
			aiming = false;
						
			myCharMove->GravityScale = normalGravity;
			UGameplayStatics::PlaySoundAtLocation(world, landSFX, GetActorLocation(), SFXvolume);
			MakeNoise(1.0f, this, GetActorLocation());			
			if(debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("[player] called stopLand, playerState: %d"), mystate));			
			GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::StopLand, landTime, false);
		}
		grounded = true;
		airJumpLocked = false;
		airAttackLocked = false;
		myAnimBP->airdashed = false;
		myAnimBP->jumped = false;
		myAnimBP->airJump = false;
		myAnimBP->airJumped = false;
	}
	else{
		if (grounded) {
			//just lost the ground			
			if (mystate == attacking) {
				CancelAttack();
				mystate = idle;
			}
		}
		grounded = false;
	}
	myRotation = Controller->GetControlRotation();
	YawRotation = FRotator(0, myRotation.Yaw, 0);
	
	if (dashStart > 0 && mytime - dashStart < dashTime && dashPower > 0.0f) {
		//dashing
		if (!lookInCamDir) {
			myCharMove->bOrientRotationToMovement = false;
			//look in camera direction, projected on the ground plane
			FRotator CamRotation = Controller->GetControlRotation();
			CamRotation.Pitch = 0;
			CamRotation.Roll = 0;
			SetActorRotation(CamRotation);
		}
				
		dashing = true;
		startRecoverStamina = 0.0f;
		//if forward, then it is the infinite dash, no invincibility
		if (vertIn > 0 && FMath::Abs(horIn) <= 0.5f && mystate != evading) {
			mystate = idle;
			//idleTimer = back2idleTime;
			if(!dashLocked)
				UGameplayStatics::PlaySoundAtLocation(world, dashStartSFX, GetActorLocation(), SFXvolume);
			dashLocked = true;
			//cancel the timer
			dashStart = mytime;
			dashPowerRate = dashForthRate;
			dashDirH = 0.0f;
			dashDirV = 1.0f;
			turboThrustVFX->Activate();
			mainThrustVFX->Activate();
		}
		else{
			if(!dashLocked){
				//if no move in axe input, evade backwards
				if (vertIn == 0.0f && horIn == 0.0f) {
					UGameplayStatics::PlaySoundAtLocation(world, evadeSFX, GetActorLocation(), SFXvolume);
					mystate = evading;
					dashPowerRate = evadeRate;
					dashDirV = -1.0f;
					dashDirH = 0.0f;
				}
				else {
					//evade diagonal
					mystate = evading;
					UGameplayStatics::PlaySoundAtLocation(world, evadeSFX, GetActorLocation(), SFXvolume);
					dashPowerRate = evadeRate;
					dashDirV = vertIn;
					dashDirH = horIn;
					//turboThrustVFX->Activate();
				}
				dashLocked = true;
			}
		}
		dashCooldownTimer = 0.0f;		
	}
	else {
		dashing = false;
		turboThrustVFX->Deactivate();
		mainThrustVFX->Deactivate();
		dashPowerRate = recoverStaminaRate;
		if (dashStart != 0.0f) {
			if (debugInfo)
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Blue, TEXT("just finished dashing1!"));
			//ResetSpeeds();
			myCharMove->GravityScale = normalGravity;
			dashCooldownTimer = dashCooldownTime;
			
			startRecoverStamina = mytime;
			if (inAir)
				myAnimBP->airdashed = true;
		}
		dashStart = 0.0f;
	}
	if(!dashDesire && mystate != evading) {	
		//dash button released, stop turboForth
		turboThrustVFX->Deactivate();
		mainThrustVFX->Deactivate();
		dashLocked = false;
		dashing = false;
		//ResetSpeeds();
		
		dashPowerRate = recoverStaminaRate;
		if (dashStart != 0.0f) {
			if (debugInfo)
				GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Blue, TEXT("just finished dashing!"));
			dashCooldownTimer = dashCooldownTime;
			myCharMove->GravityScale = normalGravity;
			startRecoverStamina = mytime;
			if (inAir)
				myAnimBP->airdashed = true;
		}
		dashStart = 0.0f;
	}

	myVel = GetVelocity();
		
	if (lookInCamDir) {
		//using always the normal speed to normalize, so the dash animation is max out			
		myAnimBP->speedv = 100.0f*dashGain4Anim*(FVector::DotProduct(myVel, forthVec) / myCharMove->MaxWalkSpeed);
		myAnimBP->speedh = 100.0f*dashGain4Anim*(FVector::DotProduct(myVel, rightVec) / myCharMove->MaxWalkSpeed);
	}
	else {
		//using always the normal speed to normalize, so the dash animation is max out
		if (dashing) {
			myAnimBP->speedv = 100.0f*dashGain4Anim*(FVector::DotProduct(myVel, forthVec) / myCharMove->MaxWalkSpeed);
			myAnimBP->speedh = 100.0f*dashGain4Anim*(FVector::DotProduct(myVel, rightVec) / myCharMove->MaxWalkSpeed);
		}
		else {
			myAnimBP->speedv = 100.0f*dashGain4Anim*(FVector::VectorPlaneProject(myVel, FVector::UpVector).Size() / myCharMove->MaxWalkSpeed);
			myAnimBP->speedh = 0.0f;
		}
	}	

	if(dashCooldownTimer > 0)
		dashCooldownTimer -= DeltaTime;

	if (dashing) {
		dashPower -= dashPowerRate * DeltaTime;
		if (dashPower <= 0) {
			dashing = false;
			turboThrustVFX->Deactivate();
			mainThrustVFX->Deactivate();
			dashPowerRate = recoverStaminaRate;
		}
		myCharMove->MaxWalkSpeed = dashSpeed;
		myCharMove->MaxAcceleration = dashAcel;
		dashGain4Anim = 1.0f;
		//if(!lookInCamDir)
		//	myCharMove->bOrientRotationToMovement = false;
		myCharMove->AirControl = dashAirCtrl;
	}
	else{	
		myCharMove->MaxWalkSpeed = (dashCooldownTimer/dashCooldownTime)*(dashSpeed-normalSpeed) + normalSpeed;
		myCharMove->MaxAcceleration = (dashCooldownTimer / dashCooldownTime)*(dashAcel - normalAcel) + normalAcel;
		dashGain4Anim = (dashCooldownTimer / dashCooldownTime)*(1.0f - dashAnimGain) + dashAnimGain;
		//myCharMove->bOrientRotationToMovement = true;
		myCharMove->AirControl = normalAirCtrl;
		if (mystate == evading || (dashDirH==0.0f && dashDirV == 1.0f)) {
			mystate = idle;
			//idleTimer = back2idleTime;
			if(inAir)
				myCharMove->Velocity *= (normalSpeed / dashSpeed);
			dashDirH = 0.0f;
			dashDirV = 0.0f;
		}
		if(startRecoverStamina > 0.0f && mytime-startRecoverStamina > recoverStaminaDelay && dashPower < 1.0f){
			dashPower += dashPowerRate * DeltaTime;
		}
	}	
	/*
	//test rotation
	if (player->WasInputKeyJustPressed(EKeys::R)) {
		FRotator NewRotation = FRotator(0.0f, 90.0f, 0.0f);
		FQuat QuatRotation = FQuat(NewRotation);
		AddActorLocalRotation(QuatRotation, false, 0, ETeleportType::None);
	}
	*/

	forthVec = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	rightVec = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		
	if (!myAnimBP)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Red, TEXT("Not finding the anim blueprint"));
	}
	myAnimBP->inAir = inAir;
	myAnimBP->dash = dashing;
	/*
	if (idleTimer > 0) {
		GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Yellow, TEXT("blending..."));
		idleTimer -= DeltaTime;
		if (idleTimer < 0.0f) {
			idleTimer = 0.0f;
		}
	}
	myAnimBP->idleBlend = 1.0f - idleTimer / back2idleTime;
	*/

	//UE_LOG(LogTemp, Warning, TEXT("player state: %d | attackChain size: %d"), (int)mystate, attackChain.Num());

	/*
	if (player->WasInputKeyJustPressed(EKeys::O)) {
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, "[gamestate]first Mutation found name: " + myGameState->mutations[0]->GetName());
	}
	*/
	/*
	if (player->IsInputKeyDown(EKeys::I)) {
		player->ProjectWorldLocationToScreen(myGameState->mutations[target_i]->GetActorLocation(), targetScreenPos, false);
		targetScreenPos.X /= width;
		targetScreenPos.Y /= height;
		FVector targetdir = myGameState->mutations[target_i]->GetActorLocation() - GetActorLocation();
		targetdir.Normalize();
		const FRotator Rotation = Controller->GetControlRotation();
		FVector forthVec = FRotationMatrix(Rotation).GetUnitAxis(EAxis::X);
		float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(targetdir, forthVec)));
		float angle2 = FMath::RadiansToDegrees(FMath::Acos(forthVec.CosineAngle2D(targetdir)));
		UE_LOG(LogTemp, Warning, TEXT("target at: X: %f Y: %f|angle: %f| angle: %f"), targetScreenPos.X, targetScreenPos.Y, angle, angle2);
	}
	*/
	
	
	/*
	//array tests:
	if (player->WasInputKeyJustPressed(EKeys::J)) {
		myGameState->mutations.RemoveAt(0);
	}
	if (player->WasInputKeyJustPressed(EKeys::K)) {
		myGameState->mutations.RemoveAt(1);
	}
	if (player->WasInputKeyJustPressed(EKeys::L)) {
		myGameState->mutations.RemoveAt(2);
	}
	*/

	/*
	//if (player->WasInputKeyJustPressed(player->PlayerInput->ActionMappings[7].Key))
	//if(player->WasInputKeyJustPressed(EKeys::Y))
	//GEngine->AddOnScreenDebugMessage(-1, 0.01f, FColor::Yellow, FString::Printf(TEXT("horizontal: %f"), player->GetInputAnalogKeyState(horizontal_R)+ (-1)*player->GetInputAnalogKeyState(horizontal_L)));
	//GEngine->AddOnScreenDebugMessage(-1, 0.01f, FColor::Yellow, FString::Printf(TEXT("horizontal cam: %f"), player->GetInputAnalogKeyState(horizontalCam)));
	
	if (player->WasInputKeyJustPressed(shieldKey))
	{
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, TEXT("pressed shield key..."));
	}
	if (player->WasInputKeyJustPressed(dashKey))
	{
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, TEXT("pressed dash key..."));		
	}
	if (player->WasInputKeyJustPressed(hookKey))
	{
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, TEXT("pressed hook key..."));
	}
	*/	
	
	if (debugInfo) {
		if (player->WasInputKeyJustPressed(EKeys::J)) {
			GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Red, TEXT("[artificial damage] no KD, no spiral!"));
			//only call damage if not already in some damage state
			if (mystate < evading || mystate > kdRise) {
				//calculate damagedir on the plane to avoid a downwards vector, which could break the
				//knock down takeoff
				damageDir = -GetActorForwardVector();
				MyDamage(0.0f, false, false);
			}
		}
		if (player->WasInputKeyJustPressed(EKeys::K)) {
			GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Red, TEXT("[artificial damage] yes KD, no spiral!"));
			//only call damage if not already in some damage state
			if (mystate < evading || mystate > kdRise) {
				//calculate damagedir on the plane to avoid a downwards vector, which could break the
				//knock down takeoff
				damageDir = -GetActorForwardVector();
				MyDamage(0.0f, true, false);
			}
		}
		if (player->WasInputKeyJustPressed(EKeys::L)) {
			GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Red, TEXT("[artificial damage] yes KD, yes spiral!"));
			//only call damage if not already in some damage state
			if (mystate < evading || mystate > kdRise) {
				//calculate damagedir on the plane to avoid a downwards vector, which could break the
				//knock down takeoff
				damageDir = -GetActorForwardVector();
				MyDamage(0.0f, true, true);
			}
		}
	}
	
	thrustAudio = 0.0f;

	switch (mystate) {
	case idle:
		//myCharMove->GravityScale = normalGravity;
		/*
		if (player->WasInputKeyJustPressed(EKeys::T)) {
			const FVector SpawnPosition = GetActorLocation() + GetActorForwardVector()*50.0f;
			FActorSpawnParameters SpawnInfo;

			AMosquitoCharacter* newMosquito;
			if (player->IsInputKeyDown(EKeys::B)) {
				// spawn new mosquito using blueprint
				newMosquito = GWorld->SpawnActor<AMosquitoCharacter>(MosquitoClass, SpawnPosition, FRotator::ZeroRotator, SpawnInfo);

			}
			else {
				//newMosquito = GWorld->SpawnActor<AMosquitoCharacter>(myGameState->mutations[0]->GetClass(), SpawnPosition, FRotator::ZeroRotator, SpawnInfo);
				// spawn new mosquito without blueprint
				newMosquito = GWorld->SpawnActor<AMosquitoCharacter>(AMosquitoCharacter::StaticClass(), SpawnPosition, FRotator::ZeroRotator, SpawnInfo);
			}

			if (newMosquito != nullptr)
			{
				DrawDebugLine(world, GetActorLocation(), SpawnPosition, FColor(0, 255, 0), true, -1, 0, 5.0);
			}
			else
			{
				// Failed to spawn actor!
				if (debugInfo)
					GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Red, TEXT("failed to place new mosquito!"));
			}
		}
		*/
		
		if (!aiming && !landing) {
			Listen4Attack();
			Listen4Grab();
		}
		Listen4Dash();
		Listen4Hook();
		
		Listen4Move(DeltaTime);
		Listen4Look();
		Reorient();
		break;
	case attacking:
		jetBackUp->SetVisibility(false);
		jetBackLow->SetVisibility(false);
		jetBackL->SetVisibility(false);
		jetBackR->SetVisibility(false);
		jetLegL->SetVisibility(false);
		jetLegR->SetVisibility(false);

		Advance();
		Listen4Attack();
		Listen4Look();
		Listen4Dash();
		Reorient();
		if (updateAtkDir) {
			if (lookInCamDir) {
				Look2Dir(forthVec);
			}
			else {
				if (horIn != 0.0f || vertIn != 0.0f)
					Look2Dir(vertIn*forthVec + horIn * rightVec);
			}
		}
		break;
	case hookFlying:
		if (hookedActor) {
			myLoc = GetActorLocation();
			targetLoc = hookedActor->GetActorLocation();
			targetDir = targetLoc - myLoc;
			distToTarget = targetDir.Size();
			targetDir.Normalize();

			//set player orientation
			//myCharMove->bOrientRotationToMovement = false;
			lookToTarget = UKismetMathLibrary::FindLookAtRotation(myLoc, targetLoc);
			lookToTarget.Pitch = 0;
			lookToTarget.Roll = 0;
			SetActorRotation(lookToTarget);

			if (distToTarget < disengageHookDist)
			{
				if(oldTargetLocked)
					GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, FString::Printf(TEXT("trying to reactivate targetLocked, target_i: %d"), target_i));
				
				//get hook back
				//stop movement
				Cast<UPrimitiveComponent>(hookComp)->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				Cast<UPrimitiveComponent>(hookComp)->SetPhysicsLinearVelocity(FVector::ZeroVector);
				hookComp->AttachToComponent(myMesh, FAttachmentTransformRules::KeepRelativeTransform, hookSocket);
				hookComp->SetRelativeLocation(hookRelPos);
				hookComp->SetRelativeRotation(hookRelRot);
				//deactivate chain
				hookChain->bEnableStiffness = true;
				//hookChain->Deactivate();
				simChainCable = false;				

				//release mutation and set it to be dizzy
				AMutationChar* maybeMutation;
				if(hookedActor != nullptr) {
					maybeMutation = Cast<AMutationChar>(hookedActor);
					if (maybeMutation != nullptr) {
						maybeMutation->FromGrappleRecover(mutationDizzyTime);
						target_i = -1;
						/*
						//auto activate the target lock after a successfull hook flight
						target_i = maybeMutation->mutation_i;
						GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, FString::Printf(TEXT("trying to auto activate targetLocked, target_i: %d"), target_i));
						if (target_i >= 0) {
							cameraArmLengthTarget = CameraLockArmLength;
							nearCamStart = mytime;
							targetLocked = true;
						}
						*/
					}
				}
				hookedActor = nullptr;

				ResetAnims();
				GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Red, TEXT("returning to idle from hook flight!"));
				
				world->GetTimerManager().ClearTimer(timerHandle);
				mystate = idle;
				//go up to allow some time after releasing the grappled target
				//ResetSpeeds();
				flying = false;
				myCharMove->MovementMode = MOVE_Falling;
				myCharMove->MaxWalkSpeed = normalSpeed;
				myCharMove->MaxFlySpeed = normalSpeed;
				myCharMove->MaxAcceleration = normalAcel;
				myCharMove->AirControl = normalAirCtrl;
				myCharMove->GravityScale = hookReleaseGravity;
				myCharMove->bOrientRotationToMovement = !lookInCamDir;

				//reset gravity after hookReleaseReactTime
				GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::ResetSpeeds, hookReleaseReactTime, false);
				myCharMove->Velocity = hookReleaseUpVel*FVector::UpVector;
				UGameplayStatics::PlaySoundAtLocation(world, hookFreeSFX, GetActorLocation(), SFXvolume);
			}
			else
			{
				flying = true;
				myCharMove->MovementMode = MOVE_Flying;
				myCharMove->GravityScale = 0.0f;
				myCharMove->MaxWalkSpeed = dashSpeed * hookFlightGain * (distToTarget) / hookRange;
				myCharMove->MaxFlySpeed = dashSpeed * hookFlightGain * (distToTarget) / hookRange;
				myCharMove->MaxAcceleration = dashAcel;
				//AddMovementInput(targetDir, 1.0f);
				myCharMove->Velocity = targetDir * hookFlightGain * dashSpeed * (distToTarget) / hookRange;
				//myCharMove->Velocity = forthVec * throwPower;
				myCharMove->AirControl = 0.0f;
				
				//DrawDebugLine(world, myMesh->GetSocketLocation(hookSocket), hookPos, FColor::Blue, true, -1, 0, 5.0);
				//DrawDebugSphere(world, hookPos, 10.0f, 20, FColor(255, 0, 0), true, -1, 0, 2);
				//DrawDebugSphere(world, myMesh->GetSocketLocation(hookSocket), 10.0f, 20, FColor(0, 255, 0), true, -1, 0, 2);
			}
		}
		Listen4Look();
		Listen4Dash();
		Reorient();
		break;
	case evading:
		Listen4Look();
		Listen4Dash();
		MoveForward(dashDirV);
		MoveRight(dashDirH);
		Reorient();
		break;
	case suffering:
		jetBackUp->SetVisibility(false);
		jetBackLow->SetVisibility(false);
		jetBackL->SetVisibility(false);
		jetBackR->SetVisibility(false);
		jetLegL->SetVisibility(false);
		jetLegR->SetVisibility(false);

		if (recoilValue >= 0.0f) {
			AddMovementInput(damageDir, recoilValue);
		}
		Listen4Look();
		Reorient();
		break;
	case kdTakeOff:
		jetBackUp->SetVisibility(false);
		jetBackLow->SetVisibility(false);
		jetBackL->SetVisibility(false);
		jetBackR->SetVisibility(false);
		jetLegL->SetVisibility(false);
		jetLegR->SetVisibility(false);
		if (recoilValue >= 0.0f) {
			AddMovementInput(damageDir + knockDownUpFactor*FVector::UpVector, recoilValue);
		}
		Listen4Look();
		break;
	case kdFlight:
		//check for fast recover, if right before start falling down
		myVel = GetVelocity();
		if (FMath::Abs(myVel.Z) < quickRecoverSpeedTgt) {
			if (skillLevel >= 2 && (player->WasInputKeyJustPressed(dashKey) || player->WasInputKeyJustPressed(dash_jKey))) {
				CancelAttack();
				myAnimBP->damageIndex = 25;
				mystate = kdGround;
				myCharMove->StopMovementImmediately();
				myCharMove->StopActiveMovement();
				myCharMove->MovementMode = MOVE_Flying;
				UGameplayStatics::PlaySoundAtLocation(world, quickRecoverSFX, GetActorLocation(), SFXvolume);
				world->GetTimerManager().ClearTimer(timerHandle);
				GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::Relax, quickRecoverTime, false);
			}
			if (skillLevel < 2 && (player->IsInputKeyDown(dashKey) || player->IsInputKeyDown(dash_jKey))) {
				skillLevel = 2;
				CancelAttack();
				myAnimBP->damageIndex = 25;
				mystate = kdGround;
				myCharMove->StopMovementImmediately();
				myCharMove->StopActiveMovement();
				myCharMove->MovementMode = MOVE_Flying;
				UGameplayStatics::PlaySoundAtLocation(world, quickRecoverSFX, GetActorLocation(), SFXvolume);
				world->GetTimerManager().ClearTimer(timerHandle);
				GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::Relax, quickRecoverTime, false);
			}
		}
		//otherwise, only recover when hitting some ground
		if (!inAir) {
			mystate = kdGround;
			myAnimBP->damageIndex = 30;
			UGameplayStatics::PlaySoundAtLocation(world, KDhitGroundSFX, GetActorLocation(), SFXvolume);
			if(debugInfo)
				GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Red, TEXT("[kdFlight] hit ground"));
			GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::DelayedKDground, recoverTime, false);
		}
		Listen4Look();
		Reorient();
		break;
	case kdGround:
		Listen4Look();
		Reorient();
		break;
	case kdRise:
		if (!arising) {
			//simple rise, seesaw attack or spin attack
			if ((player->IsInputKeyDown(atk1Key) && player->WasInputKeyJustPressed(hookKey)) || (player->IsInputKeyDown(atk1_jKey) && player->WasInputKeyJustPressed(hook_jKey))) {
				CancelAttack();
				arising = true;
				//spin rise attack
				attackPower = attackKDPower;
				knockingDown = true;
				attackPush = spinRisePushForce;
				atkPushTime = spinRisePushTime;
				myAnimBP->damageIndex = 33;
				mystate = kdGround;
				UGameplayStatics::PlaySoundAtLocation(world, spinRiseSFX, GetActorLocation(), SFXvolume);
				if (swordComp)
					Cast<UPrimitiveComponent>(swordComp)->SetGenerateOverlapEvents(true);
				swordVFX->BeginTrails(atkTrailSocket1R, atkTrailSocket2R, ETrailWidthMode::ETrailWidthMode_FromFirst, trailWidthR);

				advanceAtk = 0.0f;
				GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::DelayedAtkRise, spinRiseTime, false);
			}
			if (player->WasInputKeyJustPressed(atk2Key) || player->WasInputKeyJustPressed(atk2_jKey)) {
				CancelAttack();
				arising = true;
				//seesaw attack rise
				attackPush = spinRisePushForce;
				atkPushTime = spinRisePushTime;
				attackPower = attackNormalPower;
				myAnimBP->damageIndex = 32;
				mystate = kdGround;
				UGameplayStatics::PlaySoundAtLocation(world, seesawRiseSFX, GetActorLocation(), SFXvolume);
				advanceAtk = 0.0f;

				GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::DelayedSeeSawRise, seesawRisePrepGain*seesawRiseTime, false);
			}
			if (vertIn > 0.0f) {
				CancelAttack();
				//simple rise
				arising = true;
				myAnimBP->damageIndex = 31;
				UGameplayStatics::PlaySoundAtLocation(world, normalRiseSFX, GetActorLocation(), SFXvolume);
				GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::Relax, riseTime, false);
			}
		}
		Listen4Look();
		Reorient();
		break;
	case grabbing:
		if (inAir) {
			myCharMove->MovementMode = MOVE_Flying;
			myCharMove->MaxWalkSpeed = normalSpeed;
			myCharMove->MaxFlySpeed = normalSpeed;
			myCharMove->MaxAcceleration = normalAcel;
		}
		Advance();
		Listen4Look();
		break;

	case hookThrowing:
		myLoc = myMesh->GetSocketLocation(hookSocket);
		hookPos += hookCurrSpeed * hookDir * DeltaTime;
		if(hookReturning) {			
			if (FVector::Distance(hookPos, myLoc) < disengageHookDist)
				HookFail();
		}
		hookComp->SetWorldLocation(hookPos);
		//DrawDebugLine(world, myLoc, hookPos, FColor::Blue, true, 0.01f, 0, 5.0);
		Listen4Look();
		break;
	}
}

// Called to bind functionality to input
void AMyPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	/*
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("Attack1", IE_Pressed,this, &AMyPlayerCharacter::Attack1Press);
	PlayerInputComponent->BindAction("Attack1", IE_Released, this, &AMyPlayerCharacter::Attack1Release);
	PlayerInputComponent->BindAction("Attack2", IE_Pressed, this, &AMyPlayerCharacter::Attack2Press);
	PlayerInputComponent->BindAction("Attack2", IE_Released, this, &AMyPlayerCharacter::Attack2Release);

	PlayerInputComponent->BindAxis("MoveForward", this, &AMyPlayerCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMyPlayerCharacter::MoveRight);
	
	 We have 2 versions of the rotation bindings to handle different kinds of devices differently
	 "turn" handles devices that provide an absolute delta, such as a mouse.
	 "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick	
	PlayerInputComponent->BindAxis("TurnRate", this, &AMyPlayerCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AMyPlayerCharacter::LookUpAtRate);
	
	PlayerInputComponent->BindAxis("Turn", this, &AMyPlayerCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AMyPlayerCharacter::LookUp);
	*/
}

void AMyPlayerCharacter::Turn(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * TurnRate);
	//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("turning!"));
}

void AMyPlayerCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * world->GetDeltaSeconds());
	//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("rate turning!"));
}

void AMyPlayerCharacter::LookUp(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * LookUpRate);
}
void AMyPlayerCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * world->GetDeltaSeconds());
}
void AMyPlayerCharacter::Reorient(){
	if (player->WasInputKeyJustReleased(targetLockKey) || player->WasInputKeyJustReleased(targetLock_jKey)) {
		targetLocking = false;
		if (lockTargetPersistent) {
			if (!targetLockUpdated) {
				LockTarget(false);
			}			
		}
		else { LockTarget(false); }
	}
	if (player->WasInputKeyJustPressed(targetLockKey) || player->WasInputKeyJustPressed(targetLock_jKey)) {
		targetLocking = true;
		if(!lockTargetPersistent || !targetLocked) {
			startReorient = mytime;
			FindEnemy(0);
			targetLockUpdated = true;
		}
		else{
			targetLockUpdated = false;
		}
	}
	if (targetLocked){
		crossHairColor = FColor::Emerald;
		if (lockTargetPersistent && targetLocking) {
			if (altHorIn < 0.0f) {
				startReorient = mytime;
				FindEnemy(3);
				targetLockUpdated = true;
			}
			if (altHorIn > 0.0f) {
				startReorient = mytime;
				FindEnemy(1);
				targetLockUpdated = true;
			}
			if (altVertIn > 0.0f) {
				startReorient = mytime;
				FindEnemy(2);
				targetLockUpdated = true;
			}
			if (altVertIn < 0.0f) {
				startReorient = mytime;
				FindEnemy(4);
				targetLockUpdated = true;
			}
		}

		if (target_i >= 0 && myGameState->mutations[target_i] != nullptr) {
			FVector targetpos = myGameState->mutations[target_i]->GetActorLocation();
			myLoc = GetActorLocation();
			//check if target position is valid, if it is too high disengage targetLock
			if (targetpos.Z < myLoc.Z + targetHeightTol) {
				targetLoc = targetpos - heightOffset_tgtLock * FVector::UpVector;
				FVector forthLoc = myLoc + (targetLoc - myLoc).Size()*forthVec;

				//turn on crosshairs
				player->ProjectWorldLocationToScreen(myGameState->mutations[target_i]->GetActorLocation(), targetScreenPos, false);
				//targetScreenPosAbs = targetScreenPos;
				targetScreenPos.X /= width;
				targetScreenPos.Y /= height;

				//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, FString::Printf(TEXT("size vector: %f"), (targetLoc - myLoc).Size()));

				//myCharMove->bOrientRotationToMovement = false;
				lookToTarget = UKismetMathLibrary::FindLookAtRotation(myLoc, targetLoc);
				//const FRotator lookToTargetYaw(0, lookToTarget.Yaw, 0);
				//SetActorRotation(lookToTargetYaw);

				//DrawDebugSphere(world, myLoc, 10.0f, 20, FColor(255, 0, 0), true, -1, 0, 2);
				//DrawDebugSphere(world, myLoc + forthVec * 50.0f + FVector(0.0f, 0.0f, 50.0f), 10.0f, 20, FColor(255, 0, 0), true, -1, 0, 2);

				if (startReorient == 0.0f) {
					targetScreenPosAbs = targetScreenPos;
					Controller->SetControlRotation(lookToTarget);
					//GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, "[oriented] ");
				}
				else {
					targetScreenPosAbs.X = -1;
					targetScreenPosAbs.Y = -1;
					//GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, "[orienting] ");
					float reorientGain = (mytime - startReorient) / reorientTime;
					if (reorientGain >= 1.0f)
						startReorient = 0.0f;

					FVector immediateTarget = forthLoc + (targetLoc - forthLoc)*reorientGain;

					if (debugInfo) {
						DrawDebugLine(world, myLoc, myLoc + forthVec * 100.0f *reorientGain, FColor(255, 0, 0), true, -1, 0, 5.0);
						DrawDebugLine(world, myLoc, immediateTarget, FColor(0, 255, 0), true, -1, 0, 5.0);
					}
					//calculate intermediate position for the smooth lock on
					FRotator almostLookToTarget = UKismetMathLibrary::FindLookAtRotation(myLoc, immediateTarget);
					Controller->SetControlRotation(almostLookToTarget);
				}
			}
			else {
				targetLocked = false;
				cameraArmLengthTarget = CameraFreeArmLength;
				nearCamStart = mytime;
				target_i = -1;
				//turn off the crosshairs
				targetScreenPos.X = -1.0f;
				targetScreenPos.Y = -1.0f;
				targetScreenPosAbs = targetScreenPos;
				//myCharMove->bOrientRotationToMovement = true;
			}
		}
	}
	else {
		//turn off the crosshairs if not aiming
		if (!aiming) {
			targetScreenPos.X = -1.0f;
			targetScreenPos.Y = -1.0f;
			targetScreenPosAbs = targetScreenPos;
			//myCharMove->bOrientRotationToMovement = true;
		}
	}	
}
void AMyPlayerCharacter::ReportNoise(USoundBase* SoundToPlay, float Volume)
{
	//If we have a valid sound to play, play the sound and
	//report it to our game
	if (SoundToPlay)
	{
		//Play the actual sound
		UGameplayStatics::PlaySoundAtLocation(world, SoundToPlay, GetActorLocation(), Volume);

		//Report that we've played a sound with a certain volume in a specific location
		MakeNoise(Volume, this, GetActorLocation());
	}

}
void AMyPlayerCharacter::ReportFinishHookThrow() {
	if (waiting4Hook){
		waiting4Hook = false;
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Green, FString::Printf(TEXT("throwing hook")));
		//send hook flying in grappleTarget direction
		//activate chain
		hookChain->Activate();
		simChainCable = true;
		hookChain->bEnableStiffness = false;
		//parent hook to the world
		hookComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		hookPos = hookComp->GetComponentLocation();

		myCharMove->StopMovementImmediately();
		myCharMove->StopActiveMovement();
		myCharMove->MovementMode = MOVE_Flying;
		
		//give it speed
		targetLoc = hookedActor->GetActorLocation();
		hookDir = targetLoc - hookPos;	
		//hookDir = FRotationMatrix(Controller->GetControlRotation()).GetUnitAxis(EAxis::X);

		//add movement component
		hookDir.Normalize();
		hookCurrSpeed = hookSpeed;

		//activate collider
		Cast<UPrimitiveComponent>(hookComp)->SetGenerateOverlapEvents(true);
		mystate = hookThrowing;
		waiting4HookCol = true;
	}
}
void AMyPlayerCharacter::ReportHookConnected() {
	if (waiting4HookConn) {
		waiting4HookConn = false;
		//start grapple flight
		mystate = hookFlying;
		Cast<UPrimitiveComponent>(hookComp)->SetGenerateOverlapEvents(false);
	}
}

void AMyPlayerCharacter::FindEnemy(int locDir) {
	//now find enemies	
	float bestValue = 0.0f;	
	float currValue;
	FVector querypos;
	bool queryVisible;
	
	if (target_i >= 0 && myGameState->mutations[target_i] != nullptr) {
		player->ProjectWorldLocationToScreen(myGameState->mutations[target_i]->GetActorLocation(), targetScreenPos, false);
		//targetScreenPosAbs = targetScreenPos;
		targetScreenPos.X /= width;
		targetScreenPos.Y /= height;
		inviewport = targetScreenPos.X > 0 && targetScreenPos.X < 1 && targetScreenPos.Y>0 && targetScreenPos.Y < 1;
		if (inviewport && myGameState->mutations[target_i]->mystate != MutationStates::grabbed) {
			switch (locDir) {
			case 0://center
				bestValue = FVector2D::Distance(targetScreenPos, FVector2D(0.5f, 0.5f));
				break;
			case 1://right
				bestValue = targetScreenPos.X;
				break;
			case 2://up
				bestValue = targetScreenPos.Y;
				break;
			case 3://left
				bestValue = targetScreenPos.X;
				break;
			case 4://down
				bestValue = targetScreenPos.Y;
				break;
			}
		}
	}
	for (int i=0; i<myGameState->mutations.Num(); ++i)
	{
		querypos = myGameState->mutations[i]->GetActorLocation();
		queryVisible = false;
			//check if inside the viewport
			player->ProjectWorldLocationToScreen(querypos, targetScreenPos, false);
			targetScreenPos.X /= width;
			targetScreenPos.Y /= height;
			inviewport = targetScreenPos.X > 0 && targetScreenPos.X < 1 && targetScreenPos.Y>0 && targetScreenPos.Y < 1;

			if (inviewport && myGameState->mutations[i]->mystate != MutationStates::grabbed) {
				//raycast to see if enemy is visible				
				myLoc = GetActorLocation();
				if (world->LineTraceSingleByChannel(hitres, FollowCamera->GetComponentLocation(), querypos, ECC_MAX, RayParams)) {
					if (debugInfo) {
						DrawDebugLine(world, FollowCamera->GetComponentLocation(), querypos, FColor::Red, true, 1.1f, 0, 5.0);
						GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Red, FString::Printf(TEXT("lockTarget raycast: %s"), *hitres.Actor.Get()->GetName()));
					}
					AMutationChar * queryMut = Cast<AMutationChar>(hitres.Actor.Get());
					if (queryMut != nullptr)
						queryVisible = true;
				}
				if (queryVisible){
					if (target_i == -1) {
						target_i = i;
						switch (locDir) {
						case 0://center
							bestValue = FVector2D::Distance(targetScreenPos, FVector2D(0.5f, 0.5f));
							break;
						case 1://right
							bestValue = targetScreenPos.X;
							break;
						case 2://up
							bestValue = targetScreenPos.Y;
							break;
						case 3://left
							bestValue = targetScreenPos.X;
							break;
						case 4://down
							bestValue = targetScreenPos.Y;
							break;
						}
					}
					else {
						switch (locDir) {
						case 0://center						
							currValue = FVector2D::Distance(targetScreenPos, FVector2D(0.5f, 0.5f));
							if (currValue < bestValue) {
								bestValue = currValue;
								target_i = i;
							}
							break;
						case 1://right
							if (targetScreenPos.X > bestValue) {
								bestValue = targetScreenPos.X;
								target_i = i;
							}
							break;
						case 2://up
							if (targetScreenPos.Y < bestValue) {
								bestValue = targetScreenPos.Y;
								target_i = i;
							}
							break;
						case 3://left
							if (targetScreenPos.X < bestValue) {
								bestValue = targetScreenPos.X;
								target_i = i;
							}
							break;
						case 4://down
							if (targetScreenPos.Y > bestValue) {
								bestValue = targetScreenPos.Y;
								target_i = i;
							}
							break;
						}
					}
				}
			}
	}
	if (target_i >= 0) {
		targetLocked = true;
		cameraArmLengthTarget = CameraLockArmLength;
		nearCamStart = mytime;
	}
	else {
		targetLocked = false;
		cameraArmLengthTarget = CameraFreeArmLength;
		nearCamStart = mytime;
	}
}
void AMyPlayerCharacter::Look2Dir(FVector LookDir) {
	myLoc = GetActorLocation();
	targetLoc = myLoc + LookDir;
	lookToTarget = UKismetMathLibrary::FindLookAtRotation(myLoc, targetLoc);
	const FRotator lookToTargetYaw(0, lookToTarget.Yaw, 0);
	SetActorRotation(lookToTargetYaw);
}
void AMyPlayerCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		if(lookInCamDir)
			Look2Dir(forthVec);
		AddMovementInput(forthVec, Value);		
	}
}

void AMyPlayerCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{	
		if (lookInCamDir)
			Look2Dir(forthVec);
		AddMovementInput(rightVec, Value);
		//myCharMove->Velocity = rightVec * Value * myspeed;
	}
}

void AMyPlayerCharacter::Attack1Press(){
	startedHold1 = mytime;//GetWorld->GetTimeSeconds();
	atk1Hold = true;
	//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Green, FString::Printf(TEXT("Time press 1: %f"), startedHold1));
}
void AMyPlayerCharacter::Attack2Press() {
	startedHold2 = mytime;
	atk2Hold = true;
	/*
	//test animation without blueprints
	GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Orange, "[ComponentName]: " + swordComp->GetName());
	GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("attack2!"));
	myMesh->PlayAnimation(attackList[1].myAnim, false);
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::ResetAnims, 1.5f, false);
	*/
}
void AMyPlayerCharacter::Attack1Release() {
	if(atk1Hold){
		if (mytime - startedHold1 < holdTimeMin) {
			if (debugInfo)
				GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("quick attack1 press"));
			
			AttackWalk(true);
			CancelKnockDownPrepare(true);
		}
		else {
			if (debugInfo)
				GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("long attack1 press"));
			atk1Hold = false;
			if(!inAir)
				KnockDownHit(true);
		}
	}
}
void AMyPlayerCharacter::Attack2Release() {
	if (atk2Hold) {
		if (mytime - startedHold2 < holdTimeMin) {
			if (debugInfo)
				GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("quick attack2 press"));
			
			AttackWalk(false);
			CancelKnockDownPrepare(false);
		}
		else {
			if (debugInfo)
				GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("long attack2 press"));
			atk2Hold = false;
			KnockDownHit(false);
		}
	}
}
void AMyPlayerCharacter::KnockDownHit(bool left) {
	//to avoid being locked 
	CancelAttack();
	//clear current relax timer
	world->GetTimerManager().ClearTimer(timerHandle);
	//old way, with the animation blueprint
	knockDownIndex = 2;
	//myAnimBP->knockDown = knockDownIndex;

	knockingDown = true;
	attackPower = attackKDPower;
	
	float relaxTime;
	if(left){ //Ground Punch
		//become lethal
		if (hookComp)
			Cast<UPrimitiveComponent>(hookComp)->SetGenerateOverlapEvents(true);
		//start the attack
		//superHitL.myAnim->RateScale = superHitL.speed;
		UGameplayStatics::PlaySoundAtLocation(world, ChargeSlashL_endSFX, GetActorLocation(), SFXvolume);
		//myMesh->PlayAnimation(superHitL.myAnim, false);
		groundPunchVFX->BeginTrails(atkTrailSocket1L, atkTrailSocket2L, ETrailWidthMode::ETrailWidthMode_FromFirst, trailWidthSuperL);
		myAnimBP->PlaySlotAnimationAsDynamicMontage(
			superHitL.myAnim,
			"DefaultSlot",
			playerTelegraph,
			10.0f,
			superHitL.speed,
			1,
			0.0f,
			0.0f);
		advanceAtk = superHitL.advanceAtkValue;
		attackPush = superHitL.pushForce;
		atkPushTime = superHitL.pushTime;
		relaxTime = superHitL.myAnim->SequenceLength/superHitL.speed;
	}
	else{ //Ranged Slash
		//become lethal
		if (swordComp)
			Cast<UPrimitiveComponent>(swordComp)->SetGenerateOverlapEvents(true);

		//superHitR.myAnim->RateScale = superHitR.speed;
		UGameplayStatics::PlaySoundAtLocation(world, ChargeSlashR_endSFX, GetActorLocation(), SFXvolume);
		rangedSwordVFX->BeginTrails(atkTrailSocket1R, atkTrailSocket2R, ETrailWidthMode::ETrailWidthMode_FromFirst, trailWidthSuperR);
		//myMesh->PlayAnimation(superHitR.myAnim, false);
		myAnimBP->PlaySlotAnimationAsDynamicMontage(
			superHitR.myAnim,
			"DefaultSlot",
			playerTelegraph,
			10.0f,
			superHitR.speed,
			1,
			0.0f,
			0.0f);
		advanceAtk = superHitR.advanceAtkValue;
		attackPush = superHitR.pushForce;
		atkPushTime = superHitR.pushTime;
		relaxTime = superHitR.myAnim->SequenceLength/superHitR.speed;
	}
	mystate = attacking;
	myAnimBP->attacking = true;
	
	if (inAir) {
		myCharMove->GravityScale = 0.0f;
		//myCharMove->StopMovementImmediately();
		//myCharMove->StopActiveMovement();		
		FVector oldMove = myCharMove->Velocity;
		oldMove = oldMove.ProjectOnTo(FVector(1.0f, 0.0f, 0.0f)) + oldMove.ProjectOnTo(FVector(0.0f, 1.0f, 0.0f));
		myCharMove->Velocity = oldMove / 2;
	}
	
	//start next relax
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::Relax, relaxTime, false);
}
void AMyPlayerCharacter::AttackWalk(bool left){
	if (inAir && airAttackLocked)
		return;
	if (!inAir && attackLocked)
		return;
		
		if(attackChain.Num() == 0){
			if (inAir) {
				atkWalker = &attackAirList[0];
			}
			else {
				atkWalker = &attackList[0];
			}
		}
		
		//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Orange, FString::Printf(TEXT("atkIndex: %d atkChainIndex: %d"), atkIndex, atkChainIndex));
		if (left) {
			if (atkWalker->left != nullptr) {
				atkWalker = atkWalker->left;
				//myCharMove->bOrientRotationToMovement = false;
				attackChain.Add(*atkWalker);				
			}
			else {
				if(inAir)
					airAttackLocked = true;
				else 
					attackLocked = true;
			}
		}
		else {
			if (atkWalker->right != nullptr) {
				atkWalker = atkWalker->right;
				//myCharMove->bOrientRotationToMovement = false;
				attackChain.Add(*atkWalker);				
			}
			else {
				if(inAir)
					airAttackLocked = true;
				else
					attackLocked = true;				
			}
		}
	
}
void AMyPlayerCharacter::NextComboHit(){
	if (atkIndex >= attackChain.Num()){
		//reset attackChain
		attackChain.Empty();
		atkIndex = 0;
		atkChainIndex = 0;
		airAttackLocked = true;
		attackLocked = false;
		if(debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::White, TEXT("reset attack chain"));
		Relax();
	}
	else {
		mystate = attacking;
		myAnimBP->attacking = true;
		updateAtkDir = true;
		knockingDown = attackChain[atkIndex].knockDown;
		attackPush = attackChain[atkIndex].pushForce;
		atkPushTime = attackChain[atkIndex].pushTime;
		if (knockingDown)
			attackPower = attackKDPower;
		else
			attackPower = attackNormalPower;
		
		//clear current relax timer
		world->GetTimerManager().ClearTimer(timerHandle);

		//reset previous
		attackChain[atkChainIndex].myAnim->RateScale = 1.0f;

		//play attack animation
		atkChainIndex = atkIndex;
		//attackChain[atkIndex].myAnim->RateScale = attackChain[atkIndex].speed;
		if(inAir){
			myCharMove->GravityScale = 0.0f;
			//myCharMove->StopMovementImmediately();
			//myCharMove->StopActiveMovement();		
			FVector oldMove = myCharMove->Velocity;
			oldMove = oldMove.ProjectOnTo(FVector(1.0f,0.0f,0.0f)) + oldMove.ProjectOnTo(FVector(0.0f, 1.0f, 0.0f));
			myCharMove->Velocity = oldMove/2;
		}
		else{
			advanceAtk = attackChain[atkIndex].advanceAtkValue;
		}
		
		myAnimBP->PlaySlotAnimationAsDynamicMontage(
			attackChain[atkIndex].myAnim, 
			"DefaultSlot", 
			playerTelegraph, 
			10.0f,//attackChain[atkIndex].coolDown,
			attackChain[atkIndex].speed,
			1, 
			0.0f,
			0.0f);
		atkChainIndex++;		

		//call timer to start being lethal
		float time2lethal = attackChain[atkIndex].time2lethal*(attackChain[atkIndex].myAnim->SequenceLength/ attackChain[atkIndex].speed);
		GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::StartLethal, time2lethal, false);
	}
}
void AMyPlayerCharacter::StartLethal(){
	UGameplayStatics::PlaySoundAtLocation(world, BasicSlashSFX, GetActorLocation(), SFXvolume);
	updateAtkDir = false;

	if (attackChain[atkIndex].leftAttack) {
		if (hookComp)
			Cast<UPrimitiveComponent>(hookComp)->SetGenerateOverlapEvents(true);

		clawVFX->BeginTrails(atkTrailSocket1L, atkTrailSocket2L, ETrailWidthMode::ETrailWidthMode_FromFirst, trailWidthL);;
	}
	else {
		if (swordComp)
			Cast<UPrimitiveComponent>(swordComp)->SetGenerateOverlapEvents(true);

		swordVFX->BeginTrails(atkTrailSocket1R, atkTrailSocket2R, ETrailWidthMode::ETrailWidthMode_FromFirst, trailWidthR);
	}

	float time2NotLethal = attackChain[atkIndex].lethalTime*(attackChain[atkIndex].myAnim->SequenceLength / attackChain[atkIndex].speed);
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::StopLethal, time2NotLethal, false);
}
void AMyPlayerCharacter::StopLethal(){
	advanceAtk = 0.0f;
	knockingDown = false;
	attackPower = attackNormalPower;
	if (swordComp)
		Cast<UPrimitiveComponent>(swordComp)->SetGenerateOverlapEvents(false);
	float time4NextHit = (1-(attackChain[atkIndex].time2lethal+attackChain[atkIndex].lethalTime))*(attackChain[atkIndex].myAnim->SequenceLength / attackChain[atkIndex].speed);
	atkIndex++;
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::NextComboHit, time4NextHit, false);
	swordVFX->EndTrails();
	clawVFX->EndTrails();
	rangedSwordVFX->EndTrails();
	groundPunchVFX->EndTrails();
}
void AMyPlayerCharacter::Relax(){
	world->GetTimerManager().ClearTimer(timerHandle);
	ResetAnims();
	ResetSpeeds();
	
	advanceAtk = 0.0f;
	//atk1Index = 0;
	//myAnimBP->attackIndex = atk1Index;
	knockDownIndex = 0;
	//myAnimBP->knockDown = knockDownIndex;
	mystate = idle;
	//idleTimer = back2idleTime;
	//if (!lookInCamDir)
	//	myCharMove->bOrientRotationToMovement = true;

	if (swordComp)
		Cast<UPrimitiveComponent>(swordComp)->SetGenerateOverlapEvents(false);
	if (hookComp)
		Cast<UPrimitiveComponent>(hookComp)->SetGenerateOverlapEvents(false);	

	swordVFX->EndTrails();
	clawVFX->EndTrails();
	rangedSwordVFX->EndTrails();
	groundPunchVFX->EndTrails();

	if (debugInfo)
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, TEXT("relaxed!"));
}
void AMyPlayerCharacter::CancelKnockDownPrepare(bool left){
	if(left){
		atk1Hold = false;
	}
	else{
		atk2Hold = false;
	}
	if (!atk1Hold && !atk2Hold) {
		knockDownIndex = 0;
		//myAnimBP->knockDown = knockDownIndex;
		//if (attackChain.Num() == 0) {
		//	Relax();
		//}		
		if (debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("knockDownAtk cancelled"));
	}
}
void AMyPlayerCharacter::CancelAttack() {
	world->GetTimerManager().ClearTimer(timerHandle);
	CancelKnockDownPrepare(true);
	CancelKnockDownPrepare(false);

	aiming = false;
	//targetScreenPos.X = -1.0f;
	//targetScreenPos.Y = -1.0f;
	//target_i = -1;
	
	//mainThrustVFX->Deactivate();
	//turboThrustVFX->Deactivate();
	swordVFX->EndTrails();
	clawVFX->EndTrails();
	rangedSwordVFX->EndTrails();
	groundPunchVFX->EndTrails();
	
	attackChain.Empty();
	atkIndex = 0;
	atkChainIndex = 0;
	airAttackLocked = false;
	attackLocked = false;
	waiting4Hook = false;
	waiting4HookCol = false;
		
	timeDilation = 1.0f;
	this->CustomTimeDilation = timeDilation;
	UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
	
	//stop being lethal
	Cast<UPrimitiveComponent>(swordComp)->SetGenerateOverlapEvents(false);
	//stop grabbing
	Cast<UPrimitiveComponent>(grabComp)->SetGenerateOverlapEvents(false);
	//stop hook/secondary weapon
	Cast<UPrimitiveComponent>(hookComp)->SetGenerateOverlapEvents(false);

	myMesh->Stop();
	ResetAnims();	
}
void AMyPlayerCharacter::ResetAnims(){
	//myMesh->SnapshotPose(lastPose);
	//lastPose.SnapshotName = "beforeIdle";
	if (myMesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint) {
		myMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		myAnimBP = Cast<UAnimComm>(myMesh->GetAnimInstance());
		
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("following the anim blueprint again!"));
	}
	myAnimBP->attacking = false;
	myAnimBP->damageIndex = 0;
	myAnimBP->attackIndex = 0;
	//myAnimBP->lastPose = lastPose;
}

void AMyPlayerCharacter::ResetAttacks(){
	
	if (!myAnimBP) {
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("could not get anim comm instance"));
		return;
	}

	myAnimBP->attackIndex = 0;
	mystate = idle;
	//idleTimer = back2idleTime;
	if (debugInfo)
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("entered ResetAttacks"));

	// Ensure the fuze timer is cleared by using the timer handle
	world->GetTimerManager().ClearTimer(timerHandle);
}

void AMyPlayerCharacter::Listen4Dash() {
	if (!dashing && interactionLevel >= 3 && (player->WasInputKeyJustPressed(dashKey) || player->WasInputKeyJustPressed(dash_jKey))) {
		dashDesire = true;
		dashStart = mytime;
		if(mystate == attacking)
			CancelAttack();
		landing = false;
	}
	if (player->WasInputKeyJustReleased(dashKey) || player->WasInputKeyJustReleased(dash_jKey)) {
		dashDesire = false;		
	}
}
void AMyPlayerCharacter::Listen4Move(float DeltaTime){
	if ( interactionLevel >= 1 && (player->WasInputKeyJustPressed(jumpKey) || player->WasInputKeyJustPressed(jump_jKey)))
	{
		wallRunDesire = true;
		if (wallRunning) {
			//disengage wallrun and jump forward
			wallRunning = false;
			myAnimBP->wallRunning = false;
			myCharMove->Velocity.Z = airJumpSpeed;
			//reset gravity
			myCharMove->GravityScale = normalGravity;
		}
		else {
			if (grounded) {
				//ground jump
				if (debugInfo)
					GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, TEXT("ground jump..."));

				UGameplayStatics::PlaySoundAtLocation(world, jumpSFX, GetActorLocation(), SFXvolume);
				myAnimBP->jumped = true;
				Jump();
			}
			else {
				if (!airJumpLocked) {
					//air jump
					GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, TEXT("air jump..."));
					UGameplayStatics::PlaySoundAtLocation(world, airJumpSFX, GetActorLocation(), SFXvolume);
					myCharMove->Velocity.Z = airJumpSpeed;
					airJumpLocked = true;
					myAnimBP->airJump = true;
					//start function to prevent the jump animation again
					GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::StopDoubleAirJump, 0.1f, false);
				}
			}
		}		
	}
	if (interactionLevel >= 1 && (player->WasInputKeyJustReleased(jumpKey) || player->WasInputKeyJustReleased(jump_jKey)))
	{		
		wallRunDesire = false;
		wallRunning = false;
		myAnimBP->wallRunning = false;
		myCharMove->GravityScale = normalGravity;
		StopJumping();
	}
	if (wallRunning) {
		AddMovementInput(wallDir, 1.0f);
	}
	else{
		MoveForward(vertIn);
		MoveRight(horIn);

		//jet effects
		jetBackUp->SetVisibility(vertIn > 0.0f);
		jetBackLow->SetVisibility(vertIn > 0.0f);
		jetBackL->SetVisibility(horIn > 0.0f);
		jetBackR->SetVisibility(horIn < 0.0f);
		jetLegL->SetVisibility(vertIn < 0.0f || horIn > 0.0f);
		jetLegR->SetVisibility(vertIn < 0.0f || horIn < 0.0f);

		//audio
		if (vertIn != 0.0f || horIn != 0.0f)
			thrustAudio = FMath::Clamp(myVel.Size() / dashSpeed, 0.1f, 1.0f);
		else
			thrustAudio = 0.0f;
	}
	
	/*
	//print speeds for animation
	if (debugInfo) {
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Orange, FString::Printf(TEXT("[player anim] speedv: %f speedh: %f move speed: %f vel: %f ratio: %f"), myAnimBP->speedv, myAnimBP->speedh, myCharMove->MaxWalkSpeed, myVel.Size(), myVel.Size()/myCharMove->MaxWalkSpeed));
	}
	*/
	
}
void AMyPlayerCharacter::Listen4Look(){
	Turn(player->GetInputAnalogKeyState(horizontalCam) + joyTurnGain * player->GetInputAnalogKeyState(horizontal_jCam));
	LookUp((-1)*player->GetInputAnalogKeyState(verticalCam)+ verticalJoyDir*player->GetInputAnalogKeyState(vertical_jCam));

	if (player->WasInputKeyJustPressed(quickTurnKey) || player->WasInputKeyJustPressed(quickTurn_j)) {
		//quickly turn 180 degrees
		FRotator quickTurnRot = FRotator(0.0f, 180.0f, 0.0f);
		FQuat quickTurnQuat = FQuat(quickTurnRot);
		AddActorLocalRotation(quickTurnQuat, false, 0, ETeleportType::None);
		Controller->SetControlRotation(quickTurnRot+Controller->GetControlRotation());
	}
	if (player->WasInputKeyJustPressed(lookInCharDir_j)) {
		//orient camera in character's direction
		forthVec = GetActorForwardVector();
		myLoc = GetActorLocation();
		targetLoc = myLoc + forthVec * 200.0f;
		lookToTarget = UKismetMathLibrary::FindLookAtRotation(myLoc, targetLoc);
		lookToTarget.Pitch = 0;
		lookToTarget.Roll = 0;
		Controller->SetControlRotation(lookToTarget);
	}
}
void AMyPlayerCharacter::Listen4Attack(){
	if (interactionLevel >= 4 && (player->WasInputKeyJustPressed(atk1Key) || player->WasInputKeyJustPressed(atk1_jKey)))
	{
		//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, TEXT("pressed atk1 key..."));
		Attack1Press();
	}
	if (interactionLevel >= 4 && (player->WasInputKeyJustReleased(atk1Key) || player->WasInputKeyJustReleased(atk1_jKey)))
	{
		//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, TEXT("released atk1 key..."));
		Attack1Release();
	}
	if (interactionLevel >= 4 && (player->WasInputKeyJustPressed(atk2Key) || player->WasInputKeyJustPressed(atk2_jKey)))
	{
		//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, TEXT("pressed atk2 key..."));
		Attack2Press();
	}
	if (interactionLevel >=4 && (player->WasInputKeyJustReleased(atk2Key) || player->WasInputKeyJustReleased(atk2_jKey)))
	{
		//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, TEXT("released atk2 key..."));
		Attack2Release();
	}
	//check knockDown
	if (attackChain.Num() == 0) {
		if (atk1Hold && mytime - startedHold1 >= holdTimeMin) {
			//try to start a knockdown prepare
			if (!inAir) {
				if ((mystate == attacking || mystate == idle) && knockDownIndex == 0) {

					world->GetTimerManager().ClearTimer(timerHandle);
					//old way, with the animation blueprint
					knockDownIndex = 1;
					//myAnimBP->knockDown = knockDownIndex;

					//start the attack
					//prepSuperHitL.myAnim->RateScale = prepSuperHitL.speed;
					UGameplayStatics::PlaySoundAtLocation(world, ChargeSlashL_iSFX, GetActorLocation(), SFXvolume);
					//myMesh->PlayAnimation(prepSuperHitL.myAnim, false);
					myAnimBP->attacking = true;
					myAnimBP->PlaySlotAnimationAsDynamicMontage(
						prepSuperHitL.myAnim,
						"DefaultSlot",
						playerTelegraph,
						10.0f,
						prepSuperHitL.speed,
						1,
						0.0f,
						0.0f);

					if (debugInfo)
						GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("knockDownAtk started"));
				}
			}
			if (mytime - startedHold1 > holdTimeMax) {
				myAnimBP->attacking = false;
				CancelKnockDownPrepare(true);
			}
			else {
				myLoc = GetActorLocation();
				forthVec = FRotationMatrix(Controller->GetControlRotation()).GetUnitAxis(EAxis::X);
				FVector targetPos = GetActorLocation() + forthVec * throwPower;
				
				//look in target direction		
				myCharMove->bOrientRotationToMovement = false;
				lookToTarget = UKismetMathLibrary::FindLookAtRotation(myLoc, targetPos);
				lookToTarget.Pitch = 0;
				lookToTarget.Roll = 0;
				SetActorRotation(lookToTarget);
			}
		}
		if (atk2Hold && mytime - startedHold2 >= holdTimeMin) {
			//try to start a knockdown prepare
			if (!inAir) {
				if ((mystate == attacking || mystate == idle) && knockDownIndex == 0) {

					world->GetTimerManager().ClearTimer(timerHandle);
					//old way, with the animation blueprint
					knockDownIndex = 1;
					//myAnimBP->knockDown = knockDownIndex;
					
					//start the attack
					//prepSuperHitR.myAnim->RateScale = prepSuperHitR.speed;
					UGameplayStatics::PlaySoundAtLocation(world, ChargeSlashR_iSFX, GetActorLocation(), SFXvolume);
					//myMesh->PlayAnimation(prepSuperHitR.myAnim, false);
					myAnimBP->attacking = true;
					myAnimBP->PlaySlotAnimationAsDynamicMontage(
						prepSuperHitR.myAnim,
						"DefaultSlot",
						playerTelegraph,
						10.0f,
						prepSuperHitR.speed,
						1,
						0.0f,
						0.0f);
					if (debugInfo)
						GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Blue, TEXT("knockDownAtk started"));
				}
			}
			if (mytime - startedHold2 > holdTimeMax) {
				myAnimBP->attacking = false;
				CancelKnockDownPrepare(false);
			}
			else {
				myLoc = GetActorLocation();
				forthVec = FRotationMatrix(Controller->GetControlRotation()).GetUnitAxis(EAxis::X);
				FVector targetPos = GetActorLocation() + forthVec * throwPower;

				//look in target direction		
				myCharMove->bOrientRotationToMovement = false;
				lookToTarget = UKismetMathLibrary::FindLookAtRotation(myLoc, targetPos);
				lookToTarget.Pitch = 0;
				lookToTarget.Roll = 0;
				SetActorRotation(lookToTarget);
			}
		}
	}
	else {
		if (atkChainIndex == 0)
			NextComboHit();
	}
}
void AMyPlayerCharacter::Advance(){	
	if ((Controller != nullptr) && (advanceAtk != 0.0f))
	{
		// get forward vector
		const FVector Direction = GetActorForwardVector();
		AddMovementInput(Direction, advanceAtk);
	}
}
void AMyPlayerCharacter::Listen4Grab() {
	//grab
	if ((player->WasInputKeyJustPressed(grabKey) || player->WasInputKeyJustPressed(grab_jKey)) && !mutationGrabbed && interactionLevel >=5) {
		mystate = grabbing;
		ResetAnims();

		myAnimBP->attackIndex = 30;
		UGameplayStatics::PlaySoundAtLocation(world, grabStartSFX, GetActorLocation(), SFXvolume);

		//look forward
		forthVec = FRotationMatrix(Controller->GetControlRotation()).GetUnitAxis(EAxis::X);
		myLoc = GetActorLocation();
		targetLoc = myLoc + forthVec * 200.0f;
		lookToTarget = UKismetMathLibrary::FindLookAtRotation(myLoc, targetLoc);
		lookToTarget.Pitch = 0;
		lookToTarget.Roll = 0;
		SetActorRotation(lookToTarget);

		advanceAtk = 1.0f;

		//turn on overlap
		if (grabComp)
			Cast<UPrimitiveComponent>(grabComp)->SetGenerateOverlapEvents(true);

		//start delayed recover to idle
		GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::DelayedMutationGrabToIdle, grabTime, false);
	}
	//grab aim
	if ((player->WasInputKeyJustPressed(grabKey) || player->WasInputKeyJustPressed(grab_jKey)) && mutationGrabbed && interactionLevel >= 5) {
		//play prepare grabThrow animation
		myAnimBP->attackIndex = 100;

		//freeze time
		timeDilation = aimTimeDilation;
		UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
	}
	//grab aiming
	if ((player->IsInputKeyDown(grabKey) || player->IsInputKeyDown(grab_jKey)) && mutationGrabbed && interactionLevel >= 5) {
		//aiming = true;
		myLoc = GetActorLocation();
		forthVec = FRotationMatrix(Controller->GetControlRotation()).GetUnitAxis(EAxis::X);
		FVector targetPos = GetActorLocation() + forthVec * throwPower;
		if (debugInfo) {
			DrawDebugLine(world, GetActorLocation(), targetPos, FColor::Emerald, true, 0.1f, 0, 5.0);
		}
		
		//look in target direction		
		myCharMove->bOrientRotationToMovement = false;
		lookToTarget = UKismetMathLibrary::FindLookAtRotation(myLoc, targetPos);
		lookToTarget.Pitch = 0;
		lookToTarget.Roll = 0;
		SetActorRotation(lookToTarget);
	}
	//throw mutation
	if ((player->WasInputKeyJustReleased(grabKey) || player->WasInputKeyJustReleased(grab_jKey)) && mutationGrabbed) {
		myAnimBP->attackIndex = 101;
		waiting4GrabThrow = true;
		timeDilation = 1.0f;
		UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
		myCharMove->GravityScale = hookReleaseGravity;
		myCharMove->bOrientRotationToMovement = !lookInCamDir;

		//reset gravity after hookReleaseReactTime
		GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::ResetSpeeds, grabReleaseReactTime, false);
		//go up a little
		myCharMove->Velocity = hookReleaseUpVel * FVector::UpVector;
	}
}
void AMyPlayerCharacter::Listen4Hook() {
	if (interactionLevel >= 2 && !landing && !mutationGrabbed && (player->WasInputKeyJustPressed(hookKey) || player->WasInputKeyJustPressed(hook_jKey))) {
		UGameplayStatics::PlaySoundAtLocation(world, grapplePrepareSFX, GetActorLocation(), SFXvolume);
		//freeze time
		timeDilation = aimTimeDilation;
		UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
		oldTargetLocked = targetLocked;
		targetLocked = false;
		cameraArmLength = CameraFreeArmLength / 2;
		cameraArmLengthTarget = CameraFreeArmLength;
		nearCamStart = mytime;
		aiming = true;
		targetScreenPos.X = 0.5f;
		targetScreenPos.Y = 0.5f;
		myAnimBP->attackIndex = -1;
	}
	//grapple aim
	if (interactionLevel >=2 && aiming && !mutationGrabbed && (player->IsInputKeyDown(hookKey) || player->IsInputKeyDown(hook_jKey))) {
		//myLoc = GetActorLocation();
		myLoc = FollowCamera->GetComponentLocation();
		forthVec = FRotationMatrix(Controller->GetControlRotation()).GetUnitAxis(EAxis::X);
		FVector targetPos = myLoc + forthVec * (hookRange + disengageHookDist);
		if (debugInfo) {
			DrawDebugLine(world, myLoc, targetPos, FColor::Green, true, 0.01f, 0, 5.0);
		}
		
		//look in target direction
		//myCharMove->bOrientRotationToMovement = false;
		lookToTarget = UKismetMathLibrary::FindLookAtRotation(myLoc, targetPos);
		lookToTarget.Pitch = 0;
		lookToTarget.Roll = 0;
		SetActorRotation(lookToTarget);

		//do the aiming with the grappables
		grappleValue = 10.0f;
		crossHairColor = FColor::Silver;
		FVector2D grapScreenPos;
		hookedMutation = nullptr;
		hookedActor = nullptr;

		//do the raycast to find if there is something grappable not ocluded in front of the camera
		if (world->LineTraceSingleByChannel(hitres, myLoc, targetPos, ECC_MAX, RayParams)) {
			//if (debugInfo)
			//	GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Red, FString::Printf(TEXT("grapplehitting: %s"), *hitres.Actor.Get()->GetName()));
			//check if mutation grappable or grappable point
			AMutationChar* maybeMutation = Cast<AMutationChar>(hitres.Actor.Get());
			if (maybeMutation != nullptr) {
				if (maybeMutation->grappable) {
					hookedMutation = maybeMutation;
					hookedActor = hitres.Actor.Get();
					//and make the correspondent UI hint green
					crossHairColor = FColor::Emerald;
				}
				else {
					//not a valid target
					hookedMutation = nullptr;
					hookedActor = nullptr;
				}
			}
			else {
				AGrappable* maybeGrapPoint = Cast<AGrappable>(hitres.Actor.Get());
				if (maybeGrapPoint != nullptr) {
					hookedActor = hitres.Actor.Get();
					//and make the correspondent UI hint green
					crossHairColor = FColor::Emerald;
				}
				else {
					//not a valid target
					hookedMutation = nullptr;
					hookedActor = nullptr;
				}
			}
		}	
	}
	//throw grappling hook
	if (interactionLevel >= 2 && aiming && !mutationGrabbed && (player->WasInputKeyJustReleased(hookKey) || player->WasInputKeyJustReleased(hook_jKey))) {
		aiming = false;
		if (hookedActor) {
			hookReturning = false;
			UGameplayStatics::PlaySoundAtLocation(world, grappleFireSFX, GetActorLocation(), SFXvolume);
			myLoc = GetActorLocation();
			targetLoc = hookedActor->GetActorLocation();
			targetDir = targetLoc - myLoc;
			distToTarget = targetDir.Size();
			
			
			mystate = hookAiming;
			this->CustomTimeDilation = 1.0f / aimTimeDilation;
			//play animation to throw hook
			myAnimBP->attackIndex = 1;

			//stop mutation actions if it is a mutation			
			if (hookedMutation)
				hookedMutation->OutOfAction();

			//stop falling if in air
			if (inAir) {
				myCharMove->Velocity = FVector::ZeroVector;
				myCharMove->MovementMode = MOVE_Flying;
			}

			//wait for the animation notifier to send hook in target's direction
			waiting4Hook = true;
			//and wait for the hook to connect, throw hook time is the max wait time to fail
			GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::HookReturn, throwHookTime, false);			
		}
		else {
			this->CustomTimeDilation = 1.0f;
			myAnimBP->attackIndex = 0;
			ResetSpeeds();
			timeDilation = 1.0f;
			UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
			UGameplayStatics::PlaySoundAtLocation(world, grappleCancelSFX, GetActorLocation(), SFXvolume);
		}
	}
}

/*
void AMyPlayerCharacter::OnCompHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if(OtherActor!=nullptr && OtherActor!=this && OtherComp != nullptr){
		if (debugInfo){
			//GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Red, FString::Printf(TEXT("I %s just hit: %s"), GetName(), *OtherActor->GetName()));
			GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Orange, "[player ComponentHit] my name: " + GetName()+"hit obj: "+ *OtherActor->GetName());
		}
	}
}
*/


void AMyPlayerCharacter::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && (OtherActor != this) && OtherComp)
	{
		if (debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Green, "[player OverlapBegin] my name: " + GetName() + "hit obj: " + *OtherActor->GetName());

		//check if attacked
		AMutationChar *myAlgoz = Cast<AMutationChar>(OtherActor);
		if (myAlgoz != nullptr) {
					
			//only call damage if not already in some damage state
			if(mystate < evading || mystate > kdRise) {
				myAlgoz->attackConnected = true;
				//calculate damagedir on the plane to avoid a downwards vector, which could break the
				//knock down takeoff
				damageDir = GetActorLocation() - OtherActor->GetActorLocation();
				damageDir = FVector::VectorPlaneProject(damageDir, FVector::UpVector);
				damageDir.Normalize();
				MyDamage(myAlgoz->damagePower, myAlgoz->knockingDown, myAlgoz->spiralAtk);
			}
		}
		else {
			//check if wall run area
			if (OtherActor->ActorHasTag(FName("wallRunnable"))) {
				if(debugInfo)
					GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Green, "[player OverlapBegin] wall");

				canWallRun = true;
				if (wallRunDesire) {	
					//check if wall is on player's left, the claw/hook side
					float dotDecision = FVector::DotProduct(forthVec, OtherActor->GetActorForwardVector());
					if (dotDecision <= 1 && dotDecision > 0) {
						wallDir = OtherActor->GetActorForwardVector();
					}
					else {
						wallDir = -OtherActor->GetActorForwardVector();
					}
					//DrawDebugLine(world, GetActorLocation(), GetActorLocation() + wallDir*2000.0f, FColor::Blue, true, -1, 0, 5.0);
					//now test if wall is on the left
					FVector linkv = FVector::VectorPlaneProject(OtherActor->GetActorLocation() - GetActorLocation(), FVector::UpVector);
					FVector pseudoUp = FVector::CrossProduct(wallDir, linkv);
					pseudoUp.Normalize();
					float anglepUp = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(pseudoUp, FVector::UpVector)));
					if (anglepUp < 10) {
						GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Orange, "[wallRun] RIGHT");
					}
					else {
						GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Orange, "[wallRun] LEFT");
						//decrease gravity
						myCharMove->GravityScale = wallRunGravFactor;
						//add a tangent speed
						myCharMove->Velocity = wallRunSpeed * wallDir;
						//set vertical speed up
						//myCharMove->Velocity.Z = airJumpSpeed / 2; //vertical movement made in animation
						//prevent sideways input
						wallRunning = true;
						//switch to wallrun animation
						myAnimBP->wallRunning = true;
					}
					/*
					GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Orange, FString::Printf(TEXT("[angleup: %f"), anglepUp));
					DrawDebugLine(world, GetActorLocation(), GetActorLocation() + linkv * 500.0f, FColor::Red, true, -1, 0, 5.0);
					DrawDebugLine(world, GetActorLocation(), GetActorLocation() + pseudoUp * 500.0f, FColor::Orange, true, -1, 0, 5.0);
					*/					
				}
			}
		}
	}
}

void AMyPlayerCharacter::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (debugInfo)
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Orange, "[player OverlapEnd] my name: " + GetName() + "hit obj: " + *OtherActor->GetName());
	/*
	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, FString::Printf(TEXT("[wallrun] tags: %d"), OtherActor->Tags.Num()));
	*/
	//check if wall run area
	if (OtherActor->ActorHasTag(FName("wallRunnable"))) {
		if (debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Green, "[player OverlapEnd] wall");

		canWallRun = false;
		
		//reset gravity
		myCharMove->GravityScale = normalGravity;

		//enable sideways input
		wallRunning = false;
		
		//switch back to falling animation
		myAnimBP->wallRunning = false;
	}
}

void AMyPlayerCharacter::HookOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(debugInfo)
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, "[hook Overlap] triggered: " + GetName() + " hit obj: " + *OtherActor->GetName());

	if (waiting4HookCol) {
		waiting4HookCol = false;
		if(hookComp)
			Cast<UPrimitiveComponent>(hookComp)->SetGenerateOverlapEvents(false);
		waiting4HookConn = true;

		//cancel hook return
		world->GetTimerManager().ClearTimer(timerHandle);

		//check if mutation or grapple point
		hookedMutation = Cast<AMutationChar>(OtherActor);
		if (hookedMutation != nullptr) {
			if (hookedMutation->grabable) {
				UGameplayStatics::PlaySoundAtLocation(world, grapConnMutationSFX, GetActorLocation(), SFXvolume);
				hookedActor = OtherActor;
				HookConnect();
				hookedMutation->Grappled();
			}
			else {
				//hook fail
				UGameplayStatics::PlaySoundAtLocation(world, grappleFailSFX, GetActorLocation(), SFXvolume);
				HookReturn();
			}
		}
		else {
			AGrappable * maybeGrapPoint = Cast<AGrappable>(OtherActor);
			if (maybeGrapPoint != nullptr) {
				UGameplayStatics::PlaySoundAtLocation(world, grapConnPointSFX, GetActorLocation(), SFXvolume);
				hookedActor = OtherActor;
				HookConnect();
			}
			else {
				//hook fail
				UGameplayStatics::PlaySoundAtLocation(world, grappleFailSFX, GetActorLocation(), SFXvolume);
				HookReturn();
			}
		}
	}
}

void AMyPlayerCharacter::MyDamage(float DamagePower, bool KD, bool Spiral) {
	HookFail();
	CancelAttack();
	landing = false;
	//stop drifting
	myCharMove->StopMovementImmediately();
	myCharMove->StopActiveMovement();

	//restart standard move
	ResetSpeeds();

	myCharMove->bOrientRotationToMovement = false;
	timeDilation = 1.0f;
	UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
		
	flying = false;

	life -= DamagePower;
	if (life <= 0)
		Death();

	recoilValue = 1.0f;
	FRotator lookToRot = UKismetMathLibrary::FindLookAtRotation(myLoc, myLoc - damageDir * 500.0f);
	const FRotator lookToYaw(0, lookToRot.Yaw, 0);
	SetActorRotation(lookToYaw);

	if (KD){		
		mystate = kdTakeOff;
		myCharMove->MovementMode = MOVE_Flying;
		myCharMove->MaxWalkSpeed = normalSpeed;
		myCharMove->MaxFlySpeed = normalSpeed;
		myCharMove->MaxAcceleration = normalAcel;
		myCharMove->AirControl = normalAirCtrl;
		myCharMove->GravityScale = normalGravity;
		arising = false;

		if (Spiral) {
			myAnimBP->damageIndex = 10;
			UGameplayStatics::PlaySoundAtLocation(world, DamageKDspiralSFX, GetActorLocation(), SFXvolume);
		}
		else {
			myAnimBP->damageIndex = 11;
			UGameplayStatics::PlaySoundAtLocation(world, DamageKDSFX, GetActorLocation(), SFXvolume);
		}

		myLoc = GetActorLocation();

		timeDilation = 0.1f;
		//hit pause, or, in this case, time dilation
		UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
		//start delayed takeoff
		GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::DelayedKDtakeOff, hitPause, false);	
	}
	else {
		mystate = suffering;
		
		int damageAnimChoice = FMath::RandRange(0, 10);
		if (damageAnimChoice <= 3) {
			myAnimBP->damageIndex = 1;
			UGameplayStatics::PlaySoundAtLocation(world, DamageSFX1, GetActorLocation(), SFXvolume);
		}
		else {
			if (damageAnimChoice <= 6) {
				myAnimBP->damageIndex = 2;
				UGameplayStatics::PlaySoundAtLocation(world, DamageSFX2, GetActorLocation(), SFXvolume);
			}
			else {
				myAnimBP->damageIndex = 3;
				UGameplayStatics::PlaySoundAtLocation(world, DamageSFX3, GetActorLocation(), SFXvolume);
			}
		}

		timeDilation = 0.1f;
		//hit pause, or, in this case, time dilation
		UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
		//start stabilize timer
		GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::DelayedStabilize, hitPause, false);
	}
}
void AMyPlayerCharacter::DelayedKDtakeOff() {
	timeDilation = 1.0f;
	UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::KnockDownFlight, takeOffTime, false);	
}
void AMyPlayerCharacter::KnockDownFlight() {
	GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, "[player] KnockDownFlight.");
	
	ResetSpeeds();
	myAnimBP->damageIndex = 20;
	mystate = kdFlight;
	if (skillLevel == 1)
		ready4newSkill = true;
}
void AMyPlayerCharacter::DelayedStabilize() {
	timeDilation = 1.0f;
	UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::Stabilize, damageTime, false);
}
void AMyPlayerCharacter::HookReturn() {
	waiting4HookCol = false;
	world->GetTimerManager().ClearTimer(timerHandle);
	Cast<UPrimitiveComponent>(hookComp)->SetGenerateOverlapEvents(false);
	//stop movement
	//ResetSpeeds();
	
	//release mutation
	if (hookedMutation)
		hookedMutation->Stabilize();

	hookedMutation = nullptr;
	hookedActor = nullptr;

	timeDilation = 1.0f;
	UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
	this->CustomTimeDilation = 1.0f;

	//play animation of hook fail
	myAnimBP->attackIndex = 12;

	//bring hook back
	myCharMove->StopMovementImmediately();
	myCharMove->StopActiveMovement();
	myCharMove->MovementMode = MOVE_Walking;
	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("[player] hook returning")));
		
	//handpos - hookpos
	FVector hookArmPos = myMesh->GetSocketLocation(hookSocket);
	hookDir = hookArmPos - hookPos;
	hookDir.Normalize();
	hookReturning = true;

	hookCurrSpeed = hookSpeed;
	UGameplayStatics::PlaySoundAtLocation(world, grappleReturnSFX, GetActorLocation(), SFXvolume);

	//release player
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::HookFail, hookReturnTime, false);
}
void AMyPlayerCharacter::HookFail(){
	if (debugInfo)
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, "[player] hook Fail.");
			
	mystate = idle;
	//idleTimer = back2idleTime;
	myAnimBP->attackIndex = 0;
	
	//finish bringing hook back
	Cast<UPrimitiveComponent>(hookComp)->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Cast<UPrimitiveComponent>(hookComp)->SetPhysicsLinearVelocity(FVector::ZeroVector);
	hookComp->AttachToComponent(myMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, hookSocket);
	//deactivate chain
	hookChain->bEnableStiffness = true;
	//hookChain->Deactivate();
	simChainCable = false;
	
	hookComp->SetRelativeLocation(hookRelPos);
	hookComp->SetRelativeRotation(hookRelRot);

	//release time
	timeDilation = 1.0f;
	UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
	this->CustomTimeDilation = 1.0f;
}
void AMyPlayerCharacter::HookConnect() {
	if (mystate == hookThrowing) {
		if(debugInfo)
			GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, "[player hook] connected.");

		//cancel the timer for the hook fail
		world->GetTimerManager().ClearTimer(timerHandle);

		//play hook connect animation, the one immediately before the pull, which triggers another anim notify
		myAnimBP->attackIndex = 11;
		hookCurrSpeed = 0.0f;

		//release time
		timeDilation = 1.0f;
		UGameplayStatics::SetGlobalTimeDilation(world, timeDilation);
		this->CustomTimeDilation = 1.0f;
	}
}
void AMyPlayerCharacter::DelayedKDground() {
	mystate = kdRise;
	
	//ready for the rises tuto
	if (skillLevel < 5)
		ready4newSkill = true;
}
void AMyPlayerCharacter::DelayedAtkRise() {
	if(debugInfo)
		GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Yellow, "[player] finished rise.");
	advanceAtk = 0.0f;
	knockingDown = false;
	attackPower = attackNormalPower;
	if (swordComp)
		Cast<UPrimitiveComponent>(swordComp)->SetGenerateOverlapEvents(false);
		
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::Relax, riseAtkCoolDown, false);
}
void AMyPlayerCharacter::DelayedSeeSawRise() {
	advanceAtk = seesawRiseAdvance;
	knockingDown = true;
	if (swordComp)
		Cast<UPrimitiveComponent>(swordComp)->SetGenerateOverlapEvents(true);

	swordVFX->BeginTrails(atkTrailSocket1R, atkTrailSocket2R, ETrailWidthMode::ETrailWidthMode_FromFirst, trailWidthR);
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::DelayedAtkRise, (1.0f- seesawRisePrepGain)*seesawRiseTime, false);
}
void AMyPlayerCharacter::DelayedMutationGrabToIdle() {
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, "[player] called grab2idle.");
	if (grabComp)
		Cast<UPrimitiveComponent>(grabComp)->SetGenerateOverlapEvents(false);
	
	myAnimBP->attackIndex = 0;	
	advanceAtk = 0.0f;
	mystate = idle;
	ResetSpeeds();
}
void AMyPlayerCharacter::Stabilize() {
	recoilValue = 0.0f;
	myAnimBP->damageIndex = 0;
	
	mystate = idle;
	//if(!lookInCamDir)
	//	myCharMove->bOrientRotationToMovement = true;
	ResetSpeeds();
}
void AMyPlayerCharacter::Death() {
	GEngine->AddOnScreenDebugMessage(-1, msgTime, FColor::Red, "[player] is dead.");
	//reload level
	UGameplayStatics::OpenLevel(this, FName(*world->GetName()), false);
}

void AMyPlayerCharacter::GrabFail() {
	if (grabComp)
		Cast<UPrimitiveComponent>(grabComp)->SetGenerateOverlapEvents(false);
	world->GetTimerManager().ClearTimer(timerHandle);
	myAnimBP->attackIndex = 31;
	UGameplayStatics::PlaySoundAtLocation(world, grabFailSFX, GetActorLocation(), SFXvolume);
	GetWorldTimerManager().SetTimer(timerHandle, this, &AMyPlayerCharacter::DelayedMutationGrabToIdle, grabTime, false);
}
void AMyPlayerCharacter::LockTarget(bool active) {
	if (!active) {
		targetLocked = active;
		cameraArmLengthTarget = CameraFreeArmLength;
		nearCamStart = mytime;
		target_i = -1;
		targetScreenPos.X = -1.0f;
		targetScreenPos.Y = -1.0f;
	}
}
void AMyPlayerCharacter::GroundPunchRipple(){
	//spawn ripple
	if (WaveRippleBP) {
		FVector ripplePosition = rippleStartPosition + rippleForwardVector * grndPchRpplForthOffset;

		//DrawDebugLine(world, ripplePosition, ripplePosition + 0.5f*grndPchRppRangeY*FVector::UpVector + 0.5f*grndPchRppRangeY*FVector::RightVector, FColor::Magenta, true, -1, 0, 5.0);

		//trace to find if there is ground here
		if (world->LineTraceSingleByChannel(hitres, ripplePosition+ grndPchRppHeightCast*FVector::UpVector, ripplePosition - grndPchRppRangeY *FVector::UpVector, ECC_Camera, RayParams)) {

			rippleStartPosition = hitres.ImpactPoint;

			//DrawDebugLine(world, rippleStartPosition, rippleStartPosition + 0.5f*grndPchRppRangeY*FVector::UpVector, FColor::Green, true, -1, 0, 5.0);
			//DrawDebugLine(world, rippleStartPosition, ripplePosition + 0.5f*grndPchRppRangeY*FVector::UpVector+ 0.5f*grndPchRppRangeY*FVector::RightVector, FColor::Orange, true, -1, 0, 5.0);
			
			//only spawn ripples if the height difference is not too big, to avoid propagating on gaps or big steps
			if ((rippleStartPosition - ripplePosition).Size() <= grndPchRppRangeY) {
				ripplePosition = rippleStartPosition - grndPchRppOffsetY * FVector::UpVector;

				FActorSpawnParameters rippleSpawnInfo;
				rippleSpawnInfo.Owner = this;

				rippleRot.Add(0.0f,FMath::RandRange(-rippleRotMax, rippleRotMax),0.0f);
				FTransform rippleTransform;
				rippleTransform.SetLocation(ripplePosition);
				rippleTransform.SetRotation(FQuat(rippleRot));

				/*
				//changing the scale of the transform does not change the scale of the actor
				float newripsize = (float)((grndPunchRipples - ripple_i + 1.0f) / grndPunchRipples);
				if (debugInfo)
					GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, FString::Printf(TEXT("[player] wave ripple size: %f"), newripsize));
				rippleTransform.SetScale3D(FVector(newripsize, newripsize, newripsize));
				*/

				AActor * newRipple;
				newRipple = world->SpawnActor<AActor>(WaveRippleBP, rippleTransform, rippleSpawnInfo);
				//newRipple.flame_i = ripple_i;
				UIntProperty* intProp = FindField<UIntProperty>(newRipple->GetClass(), "flame_i");
				if (intProp != NULL)
				{					
					intProp->SetPropertyValue_InContainer(newRipple, ripple_i);
				}

				//changing size after spawn does not work
				//newRipple->SetActorScale3D(FVector(newripsize, newripsize, newripsize));

				ripple_i++;
				if (ripple_i <= grndPunchRipples) {
					GetWorldTimerManager().SetTimer(grndPchTimerHandle, this, &AMyPlayerCharacter::GroundPunchRipple, grndPunchRippleTime, false);
				}
			}
		}
	}
}
void AMyPlayerCharacter::GrabSuccess() {
	mutationGrabbed = true;
	if(!grabThrowLearned)
		ready2LearnGrabThrow = true;
	LockTarget(false);
	UGameplayStatics::PlaySoundAtLocation(world, grabConnectSFX, GetActorLocation(), SFXvolume);
	if (grabComp)
		Cast<UPrimitiveComponent>(grabComp)->SetGenerateOverlapEvents(false);
}
void AMyPlayerCharacter::ReportGrabThrow() {
	if (waiting4GrabThrow) {
		waiting4GrabThrow = false;
		GrabThrow();
	}
}
void AMyPlayerCharacter::ReportGroundPunchHit() {
	if(debugInfo)
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue, "[player] received ground punch notify");
	ripple_i = 0;

	//hardcoded -100.0f to start on character's feet instead of the middle of his body
	rippleStartPosition = GetActorLocation() -100.0f * FVector::UpVector;;
	rippleForwardVector = GetActorForwardVector();

	rippleRot = GetActorRotation();
	rippleRot.Pitch = 0.0f;
	rippleRot.Roll = 0.0f;
	GroundPunchRipple();		
}
void AMyPlayerCharacter::GrabThrow(){
	//aiming = false;
	UGameplayStatics::PlaySoundAtLocation(world, grabThrowSFX, GetActorLocation(), SFXvolume);

	//reposition the parent
	grabbedMutation->SetActorLocation(grabComp->GetSocketLocation(grabbingHandSocket) + throwOffsetHeight * FVector::UpVector);
	grabbedMutation->SetActorRotation(grabComp->GetSocketRotation(grabbingHandSocket));
	//reparent
	grabbedMutation->myMesh->RemoveSocketOverrides(grabbingHandSocket);
	grabbedMutation->myMesh->AttachToComponent(grabbedMutation->myCapsuleComp, FAttachmentTransformRules::KeepRelativeTransform);
	//reset the offset with parent
	grabbedMutation->myMesh->SetRelativeLocation(grabbedMutation->positionOffset);
	grabbedMutation->myMesh->SetRelativeRotation(grabbedMutation->rotationOffset);

	//throw him
	forthVec = FRotationMatrix(Controller->GetControlRotation()).GetUnitAxis(EAxis::X);
	grabbedMutation->GrabThrow(forthVec, throwPower);
	
	grabbedMutation = nullptr;
	mutationGrabbed = false;
}
void AMyPlayerCharacter::StopDoubleAirJump() {
	myAnimBP->airJumped = true;
}
void AMyPlayerCharacter::StopLand() {
	landing = false;
}
void AMyPlayerCharacter::ResetSpeeds() {
	dashCooldownTimer = 0.0f;
	dashGain4Anim = dashAnimGain;
	flying = false;
	myCharMove->MovementMode = MOVE_Walking;
	myCharMove->MaxWalkSpeed = normalSpeed;
	myCharMove->MaxFlySpeed = normalSpeed;
	myCharMove->MaxAcceleration = normalAcel;
	myCharMove->AirControl = normalAirCtrl;
	myCharMove->GravityScale = normalGravity;
	myCharMove->bOrientRotationToMovement = !lookInCamDir;
	if(debugInfo)
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, "[player] speeds reset");
}
void AMyPlayerCharacter::MutationDied(int MutationID) {
	//check if player was locked to me and disengage targetlock if so
	if (target_i >= 0) {
		if (target_i == MutationID) {
			target_i = -1;
			targetLocked = false;
			cameraArmLengthTarget = CameraFreeArmLength;
			nearCamStart = mytime;
		}
		else {
			if (target_i > MutationID)
				target_i--;
		}
	}
}
void AMyPlayerCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	UE_LOG(LogTemp, Warning, TEXT("player fell from the world"));
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Black, "[player] " + GetName() + " fell from world");
	
	Death();
}