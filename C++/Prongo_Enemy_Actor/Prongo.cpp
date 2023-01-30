#include "Prongo.hpp"

over(0x020C542C, 0) const ObjectInfo objectInfo = Prongo::objectInfo;
over(0x020399D4) static constexpr const ActorProfile* profile = &Prongo::profile;

static s16 turnSteps[2] = { 900, -900 };
static s16 slowTurnSteps[2] = { 270, -270 };

static LineSensorH topSensor = {-14.0fx, 14.0fx, 32.0fx};   // (left, right, y)
static LineSensorV sideSensor = {32.0fx, 9.5fx, 16.0fx};    // (top, bottom, x)
static PointSensor bottomSensor = {0.0fx, 0.0fx};

static ActiveColliderInfo activeColliderInfo = { 

    {0, 20.0fx, 13.5fx, 16.0fx},                            //actor's origin is in the center at the bottom (x, y, halfwidth, halfheight)
    CollisionGroup::Entity,
    CollisionFlag::None,
    MAKE_GROUP_MASK(CollisionGroup::Player, CollisionGroup::Entity, CollisionGroup::Fireball),
    MAKE_FLAG_MASK_EX(CollisionFlag::None),                 //always use the ex version
    0,                                                      // always 0 because unknown

    Prongo::activeCallback 
};

enum ProngoDirection {
    Left = -8192,
    Right = 8182
};

enum ProngoAnimation {
    Walking = 0,
    Standing,
    Shock,
    Attack, 
    StandUp,
    Death
};

bool Prongo::loadResources() {

    FS::Cache::loadFile(2092 - 131, false);
    FS::Cache::loadFile(2091 - 131, false);

	return true;
}

s32 Prongo::onCreate() {

    void* modelFile = FS::Cache::loadFile(2092 - 131, false);
    void* modelAnm = FS::Cache::loadFile(2091 - 131, false);

    model.create(modelFile, modelAnm, 0, 0, 0);
    model.init(1, FrameCtrl::Looping, 0.2fx, 0);

    collisionMgr.init(this, &bottomSensor, &topSensor, &sideSensor);

    activeCollider.init(this, activeColliderInfo, 0);
    activeCollider.link();

    viewOffset = Vec2(50, 50);
    activeSize = Vec2(500.0, 1000.0);

    rotationTranslation.set(0);
	renderOffset.set(0); // set(x, y); number fx; render 
	fogFlag = false;	 //unused;
	alpha = 31;

    switchState(&Prongo::mainState);

    direction = Stage::getRandom(1);
    idleAnimationState = Stage::getRandom(2);

    homePosition = position.x;
    rotation.y = Left;
    attackable = false;

	return 1;
}

bool Prongo::updateMain() {

    updateAnimation(); // either at end or beginning

    callState();

    updateVerticalVelocity(); // gravity
    applyMovement();

    updateCollisionSensors(); // for sensors
  
    destroyInactive(0);
    return 1;
}

void Prongo::mainState() {

    if (updateStep == Func::Exit) {

        outOfRange = false;
        homeDirection = false;
        wasShocked = false;
        rotationBreak = 0;
        timeBeforeAnm = 0.0fx;
        time = 0.0fx;
        velocity.x = 0.0fx;
        return;
    }

    if (updateStep == Func::Init) {
        updateStep++;
        return;
    }

    if (updateStep == Func::Step(0)) {

        inRange = checkPlayersInOffset(range);
        bool playerInSight = (!getHorizontalDirectionToPlayer() && rotation.y > (Right-1000)) || (getHorizontalDirectionToPlayer() && rotation.y < (Left+1000)); //when Prongo completes a certain rotation, he can see Mario before finishing the rotation
        
        result = collisionMgr.collisionResult;
        CollisionMgrResult check = direction ? CollisionMgrResult::WallLeftAny : CollisionMgrResult::WallRightAny;

        outOfRange = (Math::abs(position.x - homePosition) > homeRange);  
           
        if (model.finished()) {                                     //when the animation is finished, make it choose a random idleAnimation and direction

            if (rotation.y == Left || rotation.y == Right) {        //while it's rotating don't make it change its idleAnimation or it might start walking while rotating
                idleAnimationState = Stage::getRandom(2);           //chances of getting idleAnimationState Walking are 1:3
            }

            if(idleAnimationState && rotationBreak >= 3) {          //if idleAnimation is Walking then don't make it change its direction while walking
                direction = Stage::getRandom(1);
                rotationBreak = 0;                                  //idleAnimationState can be Standing several times which could make Prongo rotate several times in row; avoid it
            }
                 
            rotationBreak++;
            model.setFrame(1);                                      //animation is finished for few frames before playing another one; so idleAnimation and pronoDirection would change several times during those frames and that way it can be avoided
        }

        switch(direction) {   //make it turn slowly either to left or right

            case 0:
                Math::stepFx(rotation.y, fx16 { Right }, fx16 { slowTurnSteps[0] }); 
                homeDirection = (position.x < homePosition);
                break;
            default:
                Math::stepFx(rotation.y, fx16 { Left }, fx16 { slowTurnSteps[0] });
                homeDirection = (position.x > homePosition);
                break;
        }

        if (bool(result & check) || !bool(result & CollisionMgrResult::GroundAny)) {  //prevent him from walking into the wall or skywalking

            idleAnimationState = Standing; 

        } 

        switch(idleAnimationState) { //in case if outOfRange is true, walking idle animation only when looking towards home
            case 0:   

                if(!outOfRange || homeDirection) {  // if it's out of range then it can walk if it's toward home
                    model.init(Walking, FrameCtrl::Standard, 0.2fx, 0);
                    velocity.x = direction ? speed  * -1 : speed;  
                    break;
                }
                
            default: 
                model.init(Standing, FrameCtrl::Standard, 0.2fx, 0);
                velocity.x = 0.0fx;
                break;
        }

        if (inRange) {

            direction = getHorizontalDirectionToPlayer();
            rotateToTarget (directionalRotationY, slowTurnSteps);

            if (playerInSight  && bool(result & CollisionMgrResult::GroundAny)) {
                velocity.x = 0.0fx;
                updateStep++;
            }
        }                                                              
    }

    if (updateStep == Func::Step(1)) {

        int backJump;
        direction ? backJump = 0.1fx : backJump = -0.1fx;
        
        quadraticJump(3.6fx, backJump, -0.3fx, 0.02fx, 0.004fx, Shock, 0.53fx, wasShocked);

        if (model.finished() && wasShocked) {
            switchState(&Prongo::attackState);
        }
    }

    return;
}

void Prongo::attackState() {

    if (updateStep == Func::Exit) {

        attackable = false;
        timeBeforeAnm = 0.0fx;
        time = 0.0fx;
        homePosition = position.x;
        range = 96.0fx;
        return;
    }

    if (updateStep == Func::Init) {

        model.init(Walking, FrameCtrl::Looping, 0.65fx, 0);
        range = 110.0fx;    // prevent prongo from being outside of range when slightly jumping back
        updateStep++;
        return;
    }

    if (updateStep == Func::Step(0)) {

        inRange = checkPlayersInOffset(range);
        inAttackRange = checkPlayersInOffset(attackRange);

        direction = getHorizontalDirectionToPlayer();
        rotateToTarget (directionalRotationY, turnSteps);
                
        if (inRange && !inAttackRange) {

            velocity.x = direction ? speed * -1.36 : speed * 1.36;

        } else if (inAttackRange) {

            distanceToPlayer = (Game::getPlayer(0)->position.x - position.x)/63;  
             
            if ((!direction && rotation.y < 0)|| (direction && rotation.y > 0)) {   //making sure prongo jumps into the direction he is facing
                distanceToPlayer *= -1;                                             //prevent a weird bhaviour of prongo jumping forward but moving back because mario is in attackRange behind him
            }
 
            velocity.x = 0.0fx;
            updateStep++;

        } else if (!inRange) {
            switchState(&Prongo::mainState);
        } 
    }

    if (updateStep == Func::Step(1)) {
        quadraticJump(6.0fx, distanceToPlayer, -0.24fx, 0.04fx, 0.004fx, Attack, 0.4fx, hasAttacked);

        if(model.finished() && hasAttacked) {
            
            hasAttacked = false;
            velocity.x = 0.0fx;
            updateStep++;
        }
    }

    if (updateStep == Func::Step(2)) {

        model.init(StandUp, FrameCtrl::Standard, 0.36fx, 1);

        if (model.getFrame() < 25) {
            attackable = true;
        } else {
            attackable = false;
        }

        if(model.finished()) {
            switchState(&Prongo::mainState);
        }
    }

    return;
}

void Prongo::deathState() {

    if (updateStep == Func::Init) {
        model.init(Death, FrameCtrl::Standard, 0.8fx, 1);
        activeCollider.unlink();
        updateStep++;
        return;
    }

    if (updateStep == Func::Step(0)) {
        
        if (model.finished()) {
            destroy(true);
        }
    } 

    return;
}

void Prongo::callState() {

    if (updateFunction != nullptr) {
        (this->*updateFunction)(); 		    //it's calling the member function meaning the current state
    }
}

void Prongo::switchState(StateFunction func) {

    if (updateFunction == func)             // if the current state is equal to the passed state, which means the state didn't change
        return;					            // then the state stays

    if (updateFunction != nullptr) {
        updateStep = -1;                    // Func::Exit
        (this->*updateFunction)();          //current state is being called with Func::Exit;
    }

    updateFunction = func;                  // the current state (updateFunction) is being set to the new state (funx)
    if (updateFunction != nullptr) {
        updateStep = 0;                     // Func::Init
        (this->*updateFunction)();          //current state is being called with Func:Init;
    }
}

void Prongo::quadraticJump(fx32 jumpPower, fx32 sideJumpPower, fx32 gravityPower, fx32 peakTime, fx32 prepTime, int anim, fx32 animSpeed, bool &jumpFinished) {

    result = collisionMgr.collisionResult;

    if (bool(result & CollisionMgrResult::GroundAny) && !jumpFinished) {            //if touching ground and hasn't jumped before... prevents it from jumping more than once

        model.init(anim, FrameCtrl::Standard, animSpeed, 1);                        //play Animation

        if (timeBeforeAnm >= prepTime) {                                            //jumps after some time has passed

            velocity.y += jumpPower;
            jumpFinished = true;
        }

        timeBeforeAnm++;

    } else if (time <= peakTime){                                                   //if jump hasn't reached peak meaning time hasn't passed peakTime

        accelV = Math::mul(gravityPower, (1.0fx - Math::div(time, peakTime)));      //the closer to peak the weaker the gravity force
        velocity.x = sideJumpPower;

    } else if (time > peakTime) {                                                   //after peak was reached

        accelV = gravityPower;
    }  
    time++;

    return;
}


void Prongo::activeCallback(ActiveCollider& self, ActiveCollider& other) {

    Prongo* prongo = static_cast<Prongo*>(self.owner);          //to get the owner of the active collider and assign it to prongo

    if (prongo->attackable) { 

        PlayerStompType playerStompType = prongo->updatePlayerStomp(self, 1.0fx, false, true);

        if (playerStompType == PlayerStompType::None) {
            StageEntity::damagePlayerCallback(self, other);
        } else {
            prongo->switchState(&Prongo::deathState);
        }

    } else {
        StageEntity::damagePlayerCallback(self, other);         //if you collide and not stomp it, you will get damaged
    }
} 

s32 Prongo::onDestroy() {
	return 1;
}