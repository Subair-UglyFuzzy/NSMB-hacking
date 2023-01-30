#pragma once

#include "nsmb.h"
#include "nitro_if.h"

class Prongo : public StageEntity3DAnm {
	
public:

	using StateFunction = Func::MemberType<void, Prongo>;

	virtual s32 onCreate() override;
	virtual bool updateMain() override;
	virtual s32 onDestroy() override;
	static bool loadResources();

	void switchState(StateFunction func);
	void callState();
	void mainState();
	void attackState();
	void deathState();

	static constexpr u16 objectID = 22;
	static constexpr ObjectInfo objectInfo = {
		0, 0,
		6, 4,
		0, 0,
		0, 0,
		CollisionSwitch::LiquidParticles,
	};

	static constexpr u16 updatePriority = objectID;
	static constexpr u16 renderPriority = objectID;
	static constexpr ActorProfile profile = {&constructObject<Prongo>, updatePriority, renderPriority, loadResources};
	static void activeCallback(ActiveCollider&, ActiveCollider&);

	StateFunction updateFunction;
	s8 updateStep;

	static const inline fx32 homeRange = 120.0fx;
	static const inline fx32 attackRange = 50.0fx;
	static const inline fx32 speed = 0.8fx;

	bool inRange; 
	bool outOfRange = false;
    bool inAttackRange; 
	bool homeDirection = false;
	bool facingMario;
	bool notfacingMario;
	bool attackable;

	fx32 range = 96.0fx;
	fx32 homePosition;
	
	u32 rotationBreak = 2;
	s32 idleAnimationState;
	s32 distanceToPlayer;

	u16 time = 0.0fx;
	u16 timeBeforeAnm = 0.0fx;
	u16 peakTime;
	u16 deathFrame;

	CollisionMgrResult result;

	bool wasShocked = false;
	bool hasAttacked = false;

	void quadraticJump(fx32 jumpPower, fx32 sideJumpPower, fx32 gravityPower, fx32 peakTime, fx32 prepTime, int anim, fx32 animSpeed, bool &jumpFinished);
};