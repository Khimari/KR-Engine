#pragma once
#include "Game/control/control.h"
#include "Game/Lara/lara_struct.h"
#include "Specific/clock.h"

struct CollisionInfo;
struct ItemInfo;

constexpr auto LARA_GRAB_THRESHOLD = ANGLE(40.0f);
constexpr auto FRONT_ARC		   = ANGLE(90.0f);

// Modern control turn alphas

constexpr auto PLAYER_STANDARD_TURN_ALPHA	  = 0.2f;
constexpr auto PLAYER_SPRINT_TURN_ALPHA		  = 0.06f;
constexpr auto PLAYER_CRAWL_TURN_ALPHA		  = 0.1f;
constexpr auto PLAYER_JUMP_PREPARE_TURN_ALPHA = 0.6f;
constexpr auto PLAYER_JUMP_TURN_ALPHA		  = 0.02f;
constexpr auto PLAYER_WADE_TURN_ALPHA		  = 0.15f;
constexpr auto PLAYER_SWIM_TURN_ALPHA		  = 0.15f;
constexpr auto PLAYER_FLY_CHEAT_TURN_ALPHA	  = 0.5f;
constexpr auto PLAYER_TURNAROUND_TURN_ALPHA	  = 0.1f;

// Tank control turn rate acceleration rates

constexpr auto LARA_TURN_RATE_ACCEL			   = ANGLE(0.25f);
constexpr auto LARA_CRAWL_MOVE_TURN_RATE_ACCEL = ANGLE(0.15f);
constexpr auto LARA_POLE_TURN_RATE_ACCEL	   = ANGLE(0.25f);
constexpr auto LARA_SUBSUIT_TURN_RATE_ACCEL	   = ANGLE(0.25f);

// Tank control turn rate maxes

constexpr auto LARA_SLOW_TURN_RATE_MAX		  = ANGLE(2.0f);
constexpr auto LARA_SLOW_MED_TURN_RATE_MAX	  = ANGLE(3.0f);
constexpr auto LARA_MED_TURN_RATE_MAX		  = ANGLE(4.0f);
constexpr auto LARA_MED_FAST_TURN_RATE_MAX	  = ANGLE(5.0f);
constexpr auto LARA_FAST_TURN_RATE_MAX		  = ANGLE(6.0f);
constexpr auto LARA_WADE_TURN_RATE_MAX		  = ANGLE(3.5f);
constexpr auto LARA_SWAMP_TURN_RATE_MAX		  = ANGLE(1.5f);
constexpr auto LARA_JUMP_TURN_RATE_MAX		  = ANGLE(1.0f);
constexpr auto LARA_REACH_TURN_RATE_MAX		  = ANGLE(0.8f);
constexpr auto LARA_SLIDE_TURN_RATE_MAX		  = ANGLE(3.70f);
constexpr auto LARA_CRAWL_TURN_RATE_MAX		  = ANGLE(1.5f);
constexpr auto LARA_CRAWL_MOVE_TURN_RATE_MAX  = ANGLE(1.75f);
constexpr auto LARA_CROUCH_ROLL_TURN_RATE_MAX = ANGLE(0.75f);
constexpr auto LARA_POLE_TURN_RATE_MAX		  = ANGLE(2.5f);

// Tank control lean rates

constexpr auto LARA_LEAN_RATE = ANGLE(1.5f);
constexpr auto LARA_LEAN_MAX  = ANGLE(11.0f);

// Tank control flex rates

constexpr auto LARA_CRAWL_FLEX_RATE = ANGLE(2.25f);
constexpr auto LARA_CRAWL_FLEX_MAX	= ANGLE(50.0f) / 2; // 2 = hardcoded number of bones to flex (head and torso).

// Heights

constexpr auto LARA_HEIGHT			  = CLICK(3) - 1; // Height in basic states.
constexpr auto LARA_HEIGHT_CRAWL	  = 350;		  // Height in crawl states.
constexpr auto LARA_HEIGHT_MONKEY	  = 850;		  // Height in monkey swing states.
constexpr auto LARA_HEIGHT_TREAD	  = 700;		  // Height in water tread states.
constexpr auto LARA_HEIGHT_STRETCH	  = 870;		  // Height in jump up and ledge hang states.
constexpr auto LARA_HEIGHT_REACH	  = 820;		  // Height in reach state.
constexpr auto LARA_HEIGHT_SURFACE	  = 800;		  // Height when resurfacing water.
constexpr auto LARA_HEADROOM		  = 160;		  // Reasonable space above head.
constexpr auto LARA_RADIUS			  = 100;
constexpr auto LARA_RADIUS_CRAWL	  = 200;
constexpr auto LARA_RADIUS_UNDERWATER = 300;
constexpr auto LARA_RADIUS_DEATH	  = 400;
constexpr auto LARA_ALIGN_VELOCITY	  = 12; // TODO: Float.

// Fall velocity thresholds

constexpr auto LARA_FREEFALL_VELOCITY	= 131.0f;
constexpr auto LARA_DAMAGE_VELOCITY		= 141.0f;
constexpr auto LARA_DEATH_VELOCITY		= 155.0f;
constexpr auto LARA_DIVE_DEATH_VELOCITY = 134.0f;
constexpr auto LARA_TERMINAL_VELOCITY	= CLICK(10);

// Swim velocities

constexpr auto LARA_SWIM_VELOCITY_ACCEL		   = 2.0f;
constexpr auto LARA_SWIM_VELOCITY_DECEL		   = 1.5f;
constexpr auto LARA_TREAD_VELOCITY_MAX		   = 17.5f;
constexpr auto LARA_SWIM_VELOCITY_MAX		   = 50.0f;
constexpr auto LARA_SWIM_INTERTIA_VELOCITY_MIN = 33.5f;

constexpr auto PLAYER_POSITION_ADJUST_MAX_TIME		  = 3 * FPS;
constexpr auto PLAYER_POSE_TIME						  = 20 * FPS;
constexpr auto PLAYER_TANK_CONTROL_RUN_JUMP_TIME	  = 22;
constexpr auto PLAYER_MODERN_CONTROL_RUN_JUMP_TIME	  = PLAYER_TANK_CONTROL_RUN_JUMP_TIME / 2;
constexpr auto PLAYER_TANK_CONTROL_SPRINT_JUMP_TIME	  = 46;
constexpr auto PLAYER_MODERN_CONTROL_SPRINT_JUMP_TIME = PLAYER_TANK_CONTROL_SPRINT_JUMP_TIME / 2;

// Status value maxes

constexpr auto LARA_AIR_MAX			  = 1800.0f;
constexpr auto LARA_AIR_CRITICAL	  = LARA_AIR_MAX / 4;
constexpr auto LARA_EXPOSURE_MAX	  = 600.0f;
constexpr auto LARA_EXPOSURE_CRITICAL = LARA_EXPOSURE_MAX / 4;
constexpr auto LARA_HEALTH_MAX		  = 1000.0f;
constexpr auto LARA_HEALTH_CRITICAL	  = LARA_HEALTH_MAX / 4;
constexpr auto LARA_POISON_MAX		  = 128.0f;
constexpr auto LARA_STAMINA_MAX		  = 120.0f;
constexpr auto LARA_STAMINA_MIN       = LARA_STAMINA_MAX / 10;
constexpr auto LARA_STAMINA_CRITICAL  = LARA_STAMINA_MAX / 2;

// Node value maxes

constexpr auto PLAYER_DRIP_NODE_MAX	  = 64.0f;
constexpr auto PLAYER_BUBBLE_NODE_MAX = 12.0f;

constexpr auto STEPUP_HEIGHT		= (int)CLICK(1.5f);
constexpr auto CRAWL_STEPUP_HEIGHT	= CLICK(1) - 1;
constexpr auto MONKEY_STEPUP_HEIGHT = (int)CLICK(1.25f);
constexpr auto BAD_JUMP_CEILING		= (int)CLICK(0.75f);
constexpr auto SHALLOW_WATER_DEPTH	= (int)CLICK(0.5f);
constexpr auto WADE_WATER_DEPTH		= STEPUP_HEIGHT;
constexpr auto SWIM_WATER_DEPTH		= CLICK(2.75f);
constexpr auto SLOPE_DIFFERENCE		= 60;

static const auto PLAYER_IDLE_STATE_IDS = std::vector<int>
{
	LS_IDLE,
	LS_CROUCH_IDLE,
	LS_CRAWL_IDLE,
	LS_MONKEY_IDLE,
	LS_HANG,
	LS_LADDER_IDLE,
	LS_POLE_IDLE,
	LS_ROPE_IDLE,
	LS_TIGHTROPE_IDLE,
	LS_ONWATER_IDLE,
	LS_UNDERWATER_IDLE
};

static const auto PLAYER_STRAFE_STATE_IDS = std::vector<int>
{
	LS_IDLE,
	LS_WALK_FORWARD,
	LS_WALK_BACK,
	LS_RUN_FORWARD,
	LS_SKIP_BACK,
	LS_STEP_LEFT,
	LS_STEP_RIGHT,
	LS_TURN_LEFT_SLOW,
	LS_TURN_RIGHT_SLOW,
	LS_JUMP_PREPARE,
	LS_JUMP_FORWARD,
	LS_JUMP_BACK,
	LS_JUMP_LEFT,
	LS_JUMP_RIGHT
};

extern LaraInfo Lara;
extern ItemInfo* LaraItem;
extern CollisionInfo LaraCollision;

void LaraControl(ItemInfo* item, CollisionInfo* coll);
void LaraAboveWater(ItemInfo* item, CollisionInfo* coll);
void LaraWaterSurface(ItemInfo* item, CollisionInfo* coll);
void LaraUnderwater(ItemInfo* item, CollisionInfo* coll);
void LaraCheat(ItemInfo* item, CollisionInfo* coll);
void AnimateItem(ItemInfo* item);
void UpdateLara(ItemInfo* item, bool isTitle);
bool UpdateLaraRoom(ItemInfo* item, int height, int xOffset = 0, int zOffset = 0);
