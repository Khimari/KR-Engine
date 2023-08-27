#include "framework.h"
#include "Objects/Generic/Object/Pushables/PushableObject_Physics.h"

#include "Game/animation.h"
#include "Game/control/flipeffect.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Objects/Generic/Object/Pushables/PushableObject.h"
#include "Objects/Generic/Object/Pushables/PushableObject_BridgeCol.h"
#include "Objects/Generic/Object/Pushables/PushableObject_Effects.h"
#include "Objects/Generic/Object/Pushables/PushableObject_Scans.h"
#include "Objects/Generic/Object/Pushables/PushableObject_Stack.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Input;

namespace TEN::Entities::Generic
{
	std::unordered_map<PushablePhysicState, std::function<void(int)>> PUSHABLES_STATES_MAP;

	constexpr auto PUSHABLE_FALL_VELOCITY_MAX = BLOCK(1 / 8.0f);
	constexpr auto PUSHABLE_WATER_VELOCITY_MAX = BLOCK(1 / 16.0f);

	constexpr auto PUSHABLE_EDGE_SPEED = 0.8f;
	constexpr auto PUSHABLE_EDGE_ANGLE_MAX = 60.0f;

	constexpr auto GRAVITY_AIR = 8.0f;
	constexpr auto GRAVITY_WATER = 4.0f;
	constexpr auto GRAVITY_ACCEL = 0.5f;

	constexpr auto PUSHABLE_FALL_RUMBLE_VELOCITY = 96.0f;

	constexpr auto WATER_SURFACE_DISTANCE = CLICK(0.5f);

	void InitializePushablesStatesMap()
	{
		if (PUSHABLES_STATES_MAP.empty() )
		{
			PUSHABLES_STATES_MAP.clear();

			PUSHABLES_STATES_MAP.emplace(PushablePhysicState::Idle, &HandleIdleState);
			PUSHABLES_STATES_MAP.emplace(PushablePhysicState::Moving, &HandleMovingState);
			PUSHABLES_STATES_MAP.emplace(PushablePhysicState::MovingEdge, &HandleMovingEdgeState);
			PUSHABLES_STATES_MAP.emplace(PushablePhysicState::Falling, &HandleFallingState);
			PUSHABLES_STATES_MAP.emplace(PushablePhysicState::Sinking, &HandleSinkingState);
			PUSHABLES_STATES_MAP.emplace(PushablePhysicState::Floating, &HandleFloatingState);
			PUSHABLES_STATES_MAP.emplace(PushablePhysicState::UnderwaterIdle, &HandleUnderwaterState);
			PUSHABLES_STATES_MAP.emplace(PushablePhysicState::WatersurfaceIdle, &HandleWatersurfaceState);
			PUSHABLES_STATES_MAP.emplace(PushablePhysicState::Sliding, &HandleSlidingState);
			PUSHABLES_STATES_MAP.emplace(PushablePhysicState::StackHorizontalMove, &HandleStackHorizontalMoveState);
		}
	}

	void HandleIdleState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		//1. Check if Lara is interacting with it
		if (Lara.Context.InteractedItem == itemNumber)
		{
			//If Lara is grabbing, check the input push pull conditions
			if (PushableIdleConditions(itemNumber))
			{
				//If all good, then do the interaction.
				if (IsHeld(In::Forward)) //Pushing
				{
					int pushAnimNumber = (pushable.isOnEdge)? 
													PushableAnimInfos[pushable.AnimationSystemIndex].EdgeAnimNumber :
													PushableAnimInfos[pushable.AnimationSystemIndex].PushAnimNumber;
					SetAnimation(LaraItem, pushAnimNumber);
				}
				else if (IsHeld(In::Back)) //Pulling
				{
					int pullAnimNumber = PushableAnimInfos[pushable.AnimationSystemIndex].PullAnimNumber;
					SetAnimation(LaraItem, pullAnimNumber);
				}

				pushable.StartPos = pushableItem.Pose.Position;
				pushable.StartPos.RoomNumber = pushableItem.RoomNumber;
				pushable.BehaviourState = PushablePhysicState::Moving;

				UnpilePushable(itemNumber);				//Cut the link with the lower pushables in the stack.
				StartMovePushableStack(itemNumber);		//Prepare the upper pushables in the stack for the move.

				ResetPlayerFlex(LaraItem);

				DeactivateClimbablePushableCollider(itemNumber);
			}
			else if (	LaraItem->Animation.ActiveState != LS_PUSHABLE_GRAB &&
						LaraItem->Animation.ActiveState != LS_PUSHABLE_PULL &&
						LaraItem->Animation.ActiveState != LS_PUSHABLE_PUSH &&
						LaraItem->Animation.ActiveState != LS_PUSHABLE_EDGE)
			{
				Lara.Context.InteractedItem = NO_ITEM;
			}
		}

		//2. Check environment
		int floorHeight;
		PushableEnvironmentState envState = CheckPushableEnvironment(itemNumber, floorHeight);

		switch (envState)
		{	
			case PushableEnvironmentState::Ground:
				
				if (floorHeight != pushableItem.Pose.Position.y)
				{
					pushableItem.Pose.Position.y = floorHeight;
					int heightdifference = floorHeight - pushableItem.Pose.Position.y;
					VerticalPosAddition(itemNumber, heightdifference);
				}

				break;

			case PushableEnvironmentState::GroundWater:
			{
				if (floorHeight != pushableItem.Pose.Position.y)
				{
					pushableItem.Pose.Position.y = floorHeight;
					int heightdifference = floorHeight - pushableItem.Pose.Position.y;
					VerticalPosAddition(itemNumber, heightdifference);
				}

				int waterheight = abs(floorHeight - pushable.WaterSurfaceHeight);
				if (waterheight > GetPushableHeight(pushableItem))
				{
					if (pushable.IsBuoyant && pushable.StackUpperItem == NO_ITEM)
					{
						pushable.BehaviourState = PushablePhysicState::Floating;
						pushable.Gravity = 0.0f;
					}
				}

				// Effects: Spawn ripples.
				DoPushableRipples(itemNumber);
			}
			break;

			case PushableEnvironmentState::Air:

				//Only pass to falling if distance to bigger than 1 click. If is small, just stuck it to the ground.
				if (abs (floorHeight - pushableItem.Pose.Position.y) > CLICK (0.75f))
				{
					pushable.BehaviourState = PushablePhysicState::Falling;
					SetPushableStopperFlag(false, pushableItem.Pose.Position, pushableItem.RoomNumber);
					DeactivateClimbablePushableCollider(itemNumber);
				}
				else
				{
					pushableItem.Pose.Position.y = floorHeight;
					int heightdifference = floorHeight - pushableItem.Pose.Position.y;
					VerticalPosAddition(itemNumber, heightdifference);
				}

			break;

			case PushableEnvironmentState::ShallowWater:
			case PushableEnvironmentState::DeepWater:

				DeactivateClimbablePushableCollider(itemNumber);
				SetPushableStopperFlag(false, pushableItem.Pose.Position, pushableItem.RoomNumber);

				if (pushable.IsBuoyant && pushable.StackUpperItem == NO_ITEM)
				{
					pushable.BehaviourState = PushablePhysicState::Floating;
					pushable.Gravity = 0.0f;
				}
				else
				{
					pushable.BehaviourState = PushablePhysicState::Sinking;
					pushable.Gravity = GRAVITY_WATER;
				}
			break;

			default:
				TENLog("Error detecting the pushable environment state during IDLE with pushable ID: " + std::to_string(itemNumber), LogLevel::Warning, LogConfig::All, false);
				break;
		}
	}

	void HandleMovingState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		bool isPlayerPulling = LaraItem->Animation.ActiveState == LS_PUSHABLE_PULL;

		int quadrantDir = GetQuadrant(LaraItem->Pose.Orientation.y);
		int newPosX = pushable.StartPos.x;
		int newPosZ = pushable.StartPos.z;

		int displaceDepth = 0;
		int displaceBox = GameBoundingBox(LaraItem).Z2;

		pushable.CurrentSoundState = PushableSoundState::None;

		displaceDepth = GetLastFrame(GAME_OBJECT_ID::ID_LARA, LaraItem->Animation.AnimNumber)->BoundingBox.Z2;
		
		if (isPlayerPulling)
		{
			displaceBox -= (displaceDepth + BLOCK(1));
		}
		else
		{
			if (pushable.isOnEdge)
			{
				displaceBox -= (displaceDepth - BLOCK(0.5f));
			}
			else
			{
				displaceBox -= (displaceDepth - BLOCK(1));
			}
		}

		//Lara is doing the pushing / pulling animation
		if (LaraItem->Animation.FrameNumber != g_Level.Anims[LaraItem->Animation.AnimNumber].frameEnd - 1)
		{
			//1. Decide the goal position
			switch (quadrantDir)
			{
				case NORTH:
					newPosZ += displaceBox;
					break;

				case EAST:
					newPosX += displaceBox;
					break;

				case SOUTH:
					newPosZ -= displaceBox;
					break;

				case WEST:
					newPosX -= displaceBox;
					break;

				default:
					break;
			}

			//2. Move pushable
			
			//Don't move the pushable if the distance is too big.
			//(it may happens because the animation bounds changes in the animation loop of push pull process).
			if (abs(pushableItem.Pose.Position.z - newPosZ) > BLOCK(0.75f))
				return;
			if (abs(pushableItem.Pose.Position.x - newPosX) > BLOCK(0.75f))
				return;


			int travelledDistance = Vector3i::Distance(pushableItem.Pose.Position, pushable.StartPos.ToVector3i());
			if (pushable.isOnEdge && travelledDistance >= BLOCK(0.5f))
			{
				pushable.BehaviourState = PushablePhysicState::MovingEdge;
				return;
			}
			
			// move only if the move direction is oriented to the action
			// So pushing only moves pushable forward, and pulling only moves backwards

			//Z axis
			if (isPlayerPulling)
			{
				if ((quadrantDir == NORTH && pushableItem.Pose.Position.z > newPosZ) ||
					(quadrantDir == SOUTH && pushableItem.Pose.Position.z < newPosZ))
				{
					pushableItem.Pose.Position.z = newPosZ;
					pushable.CurrentSoundState = PushableSoundState::Moving;
				}
			}
			else
			{
				if ((quadrantDir == NORTH && pushableItem.Pose.Position.z < newPosZ) ||
					(quadrantDir == SOUTH && pushableItem.Pose.Position.z > newPosZ))
				{
					pushableItem.Pose.Position.z = newPosZ;
					pushable.CurrentSoundState = PushableSoundState::Moving;
				}
			}

			//X axis
			if (isPlayerPulling)
			{
				if ((quadrantDir == EAST && pushableItem.Pose.Position.x > newPosX) ||
					(quadrantDir == WEST && pushableItem.Pose.Position.x < newPosX))
				{
					pushableItem.Pose.Position.x = newPosX;
					pushable.CurrentSoundState = PushableSoundState::Moving;
				}
			}
			else
			{
				if ((quadrantDir == EAST && pushableItem.Pose.Position.x < newPosX) ||
					(quadrantDir == WEST && pushableItem.Pose.Position.x > newPosX))
				{
					pushableItem.Pose.Position.x = newPosX;
					pushable.CurrentSoundState = PushableSoundState::Moving;
				}
			}

			if (pushable.WaterSurfaceHeight != NO_HEIGHT)
			{
				// Effects: Spawn ripples.
				DoPushableRipples(itemNumber);
			}
		}
		else
		{
			//Pushing Pulling animation ended
			
			//1. Realign with sector center
			pushableItem.Pose.Position = GetNearestSectorCenter(pushableItem.Pose.Position);

			//2. The pushable is going to stop here, do the checks to conect it with another Stack.
			int FoundStack = SearchNearPushablesStack(itemNumber);
			StackPushable(itemNumber, FoundStack);

			//3.: It only should do it if there is not any other pushable remaining there
			SetPushableStopperFlag(false, pushable.StartPos.ToVector3i(), pushable.StartPos.RoomNumber);

			pushable.StartPos = pushableItem.Pose.Position;
			pushable.StartPos.RoomNumber = pushableItem.RoomNumber;
			
			//5. Check environment
			int floorHeight;
			PushableEnvironmentState envState = CheckPushableEnvironment(itemNumber, floorHeight);

			switch (envState)
			{
				case PushableEnvironmentState::Ground:
				case PushableEnvironmentState::GroundWater:
					//Activate trigger
					TestTriggers(&pushableItem, true, pushableItem.Flags & IFLAG_ACTIVATION_MASK);

					// Check if it has to stop the pushing/pulling movement.
					if (!PushableAnimInfos[pushable.AnimationSystemIndex].EnableAnimLoop ||
						!IsHeld(In::Action) ||
						!PushableMovementConditions(itemNumber, !isPlayerPulling, isPlayerPulling) ||
						pushable.isOnEdge)
					{
						LaraItem->Animation.TargetState = LS_IDLE;
						pushable.BehaviourState = PushablePhysicState::Idle;
						StopMovePushableStack(itemNumber); //Set the upper pushables back to normal.
						ActivateClimbablePushableCollider(itemNumber);

						//The pushable is going to stop here, do the checks to conect it with another Stack.
						int FoundStack = SearchNearPushablesStack(itemNumber);
						StackPushable(itemNumber, FoundStack);

						pushable.CurrentSoundState = PushableSoundState::Stopping;

					}
					//Otherwise, Lara continues the pushing/pull animation movement.
				break;

				case PushableEnvironmentState::Air:
					// It's now in the air, Lara can't keep pushing nor pulling. And pushable starts to fall.
					LaraItem->Animation.TargetState = LS_IDLE;
					Lara.Context.InteractedItem = NO_ITEM;
					pushable.BehaviourState = PushablePhysicState::Falling;
					pushable.CurrentSoundState = PushableSoundState::None;

					return;
				break;

				case PushableEnvironmentState::Slope:
					//TODO: if it's a slope ground?...
					//Then proceed to the sliding state.
					break;

				case PushableEnvironmentState::ShallowWater:
				case PushableEnvironmentState::DeepWater:
					// It's still in water, but there is not ground, Lara can't keep pushing nor pulling. And pushable starts to sink.
					LaraItem->Animation.TargetState = LS_IDLE;
					Lara.Context.InteractedItem = NO_ITEM;
					pushable.BehaviourState = PushablePhysicState::Sinking;
					pushable.CurrentSoundState = PushableSoundState::None;
				break;

				default:
					TENLog("Error detecting the pushable environment state during MOVING with pushable ID: " + std::to_string(itemNumber), LogLevel::Warning, LogConfig::All, false);
				break;
			}
		}
	}

	void HandleMovingEdgeState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		int floorHeight;
		PushableEnvironmentState envState = CheckPushableEnvironment(itemNumber, floorHeight);
		
		// Calculate movement direction
		Vector3 movementDirection = (pushableItem.Pose.Position - pushable.StartPos.ToVector3i()).ToVector3();
		movementDirection.y = 0.0f;
		movementDirection.Normalize();

		// Define starting and goal positions
		Vector3 initialPos = pushable.StartPos.ToVector3() + (movementDirection * 512);
		Vector3 goalPos = pushable.StartPos.ToVector3() + (movementDirection * 1024);
		goalPos.y = pushable.StartPos.y + 1024;

		// Calculate current position based on interpolation
		Vector3 currentPos = pushableItem.Pose.Position.ToVector3();

		float& elapsedTime = pushableItem.Animation.Velocity.y;
		float interpolationValue = std::clamp(elapsedTime / PUSHABLE_EDGE_SPEED, 0.0f, 1.0f);

		currentPos = Vector3(
			InterpolateCubic(initialPos.x, initialPos.x, goalPos.x, goalPos.x, interpolationValue),
			InterpolateCubic(initialPos.y, initialPos.y, goalPos.y - 700, goalPos.y, interpolationValue),
			InterpolateCubic(initialPos.z, initialPos.z, goalPos.z, goalPos.z, interpolationValue)
		);

		// Calculate the orientation angle based on the movement direction
		float maxLeanAngle = 40.0f; // Maximum lean angle (60 degrees)
		float leanAngle = ANGLE (maxLeanAngle * interpolationValue); // Gradually increase the lean angle

		if (currentPos.y > floorHeight)
		{
			currentPos.y = floorHeight;
			pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);
		}
		else
		{
			// Manage angular leaning
			int pushableQuadrant = GetQuadrant(pushableItem.Pose.Orientation.y);
			int movementQuadrant = GetQuadrant(FROM_RAD(atan2(movementDirection.z, movementDirection.x)));
			
			movementQuadrant = (movementQuadrant + pushableQuadrant) % 4;

			switch (movementQuadrant)
			{
				case 0: //EAST
					pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, leanAngle);
					break;
				case 1: //NORTH
					pushableItem.Pose.Orientation = EulerAngles(-leanAngle, pushableItem.Pose.Orientation.y, 0);
					break;
				case 2: //WEST
					pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, -leanAngle);
					break;
				case 3: //SOUTH
					pushableItem.Pose.Orientation = EulerAngles(leanAngle, pushableItem.Pose.Orientation.y, 0);
					break;
			}
		}

		pushableItem.Pose.Position = currentPos;
		elapsedTime += DELTA_TIME;

		//Check if the movement is completed.
		if (interpolationValue >= 1.0f)
		{
			currentPos = GetNearestSectorCenter(pushableItem.Pose.Position).ToVector3();

			switch (envState)
			{
				case PushableEnvironmentState::Air:
					pushable.BehaviourState = PushablePhysicState::Falling;
					pushableItem.Animation.Velocity.y = PUSHABLE_FALL_VELOCITY_MAX / 2;
				break;

				case PushableEnvironmentState::Ground:
				case PushableEnvironmentState::GroundWater:
					pushable.BehaviourState = PushablePhysicState::Idle;
					pushableItem.Pose.Position.y = floorHeight;
					pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);
					pushableItem.Animation.Velocity.y = 0.0f;
				break;

				case PushableEnvironmentState::ShallowWater:
				case PushableEnvironmentState::DeepWater:
					pushable.BehaviourState = PushablePhysicState::Sinking;
					pushableItem.Animation.Velocity.y = PUSHABLE_WATER_VELOCITY_MAX / 2;
					
					// Effect: Water splash.
					DoPushableSplash(itemNumber);
				break;

				case PushableEnvironmentState::Slope:
					pushable.BehaviourState = PushablePhysicState::Idle;
					pushableItem.Animation.Velocity.y = 0.0f;
				break;

				default:
					TENLog("Error detecting the pushable environment state during the exit of MOVING EDGE state, with pushable ID: " + std::to_string(itemNumber), LogLevel::Warning, LogConfig::All, false);
				break;
			}			
		}
	}

	void HandleFallingState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		int floorHeight;
		PushableEnvironmentState envState = CheckPushableEnvironment(itemNumber, floorHeight);

		int FoundStack = NO_ITEM;

		switch (envState)
		{
			case PushableEnvironmentState::Air:
				//Is still falling...
				pushableItem.Pose.Position.y += pushableItem.Animation.Velocity.y;
				pushableItem.Animation.Velocity.y = std::min(	pushableItem.Animation.Velocity.y + pushable.Gravity,
																PUSHABLE_FALL_VELOCITY_MAX);

				//Fixing orientation slowly:
				PushableFallingOrientation(pushableItem);
			break;

			case PushableEnvironmentState::Ground:
			case PushableEnvironmentState::GroundWater:
				//The pushable is going to stop here, do the checks to conect it with another Stack.
				FoundStack = SearchNearPushablesStack(itemNumber);
				StackPushable(itemNumber, FoundStack);

				pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);

				//Set Stopper Flag
				if (pushable.StackLowerItem == NO_ITEM)
					SetPushableStopperFlag(true, pushableItem.Pose.Position, pushableItem.RoomNumber);

				//Activate trigger
				TestTriggers(&pushableItem, true, pushableItem.Flags & IFLAG_ACTIVATION_MASK);

				// Shake floor if pushable landed at high enough velocity.
				if (pushableItem.Animation.Velocity.y >= PUSHABLE_FALL_RUMBLE_VELOCITY)
				{
					FloorShake(&pushableItem);
					pushable.CurrentSoundState = PushableSoundState::Falling;
				}
					

				//place on ground
				pushable.BehaviourState = PushablePhysicState::Idle;
				pushableItem.Pose.Position.y = floorHeight;
				pushableItem.Animation.Velocity.y = 0;

				ActivateClimbablePushableCollider(itemNumber);
				
			break;

			case PushableEnvironmentState::Slope:
				pushable.BehaviourState = PushablePhysicState::Sliding;
				pushableItem.Pose.Position.y = floorHeight;
				pushableItem.Animation.Velocity.y = 0;
			break;

			case PushableEnvironmentState::ShallowWater:
			case PushableEnvironmentState::DeepWater:
				pushable.BehaviourState = PushablePhysicState::Sinking;

				// Effect: Water splash.
				DoPushableSplash(itemNumber);
			break;

			default:
				TENLog("Error detecting the pushable environment state during FALLING with pushable ID: " + std::to_string(itemNumber), LogLevel::Warning, LogConfig::All, false);
			break;
		}
	}

	void HandleSinkingState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		int floorHeight;
		PushableEnvironmentState envState = CheckPushableEnvironment(itemNumber, floorHeight);

		int FoundStack = NO_ITEM;

		switch (envState)
		{
			case PushableEnvironmentState::Air:
			case PushableEnvironmentState::Ground:
			case PushableEnvironmentState::Slope:
				//Set Stopper Flag
				if (pushable.StackLowerItem == NO_ITEM)
					SetPushableStopperFlag(true, pushableItem.Pose.Position, pushableItem.RoomNumber);

				pushable.BehaviourState = PushablePhysicState::Falling;
				pushable.Gravity = GRAVITY_AIR;

				break;

			case PushableEnvironmentState::ShallowWater:
			case PushableEnvironmentState::DeepWater:
				
				//Manage gravity force
				if (pushable.IsBuoyant && pushable.StackUpperItem == NO_ITEM)
				{
					pushable.Gravity = pushable.Gravity - GRAVITY_ACCEL;
					if (pushable.Gravity <= 0.0f)
					{
						pushable.BehaviourState = PushablePhysicState::Floating;
						return;
					}
				}
				else
				{
					pushable.Gravity = std::max(pushable.Gravity - GRAVITY_ACCEL, GRAVITY_WATER);
				}

				//Move Object
				pushableItem.Pose.Position.y += pushableItem.Animation.Velocity.y;
				pushableItem.Animation.Velocity.y = std::min(	pushableItem.Animation.Velocity.y + pushable.Gravity,
																	PUSHABLE_WATER_VELOCITY_MAX);
				//Fixing orientation slowly:
				PushableFallingOrientation(pushableItem);

				if (envState == PushableEnvironmentState::ShallowWater)
				{
					// Effects: Spawn ripples.
					DoPushableRipples(itemNumber);
				}
				else
				{
					// Effects: Spawn bubbles.
					DoPushableBubbles(itemNumber);
				}

				break;

			case PushableEnvironmentState::GroundWater:
				if (pushable.IsBuoyant && pushable.StackUpperItem == NO_ITEM)
				{
					pushableItem.Pose.Position.y = floorHeight;
					pushable.BehaviourState = PushablePhysicState::Floating;
					pushable.Gravity = 0.0f;
				}
				else
				{
					pushableItem.Pose.Position.y = floorHeight;
					pushable.BehaviourState = PushablePhysicState::UnderwaterIdle;
					pushable.Gravity = GRAVITY_WATER;
					pushableItem.Animation.Velocity.y = 0.0f;
					ActivateClimbablePushableCollider(itemNumber);

					pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);

					//Activate trigger
					TestTriggers(&pushableItem, true, pushableItem.Flags & IFLAG_ACTIVATION_MASK);
				}
				break;

			default:
				TENLog("Error detecting the pushable environment state during SINKING with pushable ID: " + std::to_string(itemNumber), LogLevel::Warning, LogConfig::All, false);
				break;

		}
	}

	void HandleFloatingState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		int floorHeight;
		int ceilingHeight;
		PushableEnvironmentState envState = CheckPushableEnvironment(itemNumber, floorHeight, &ceilingHeight);

		int goalHeight = 0;

		switch (envState)
		{
			case PushableEnvironmentState::Air:
			case PushableEnvironmentState::Ground:
			case PushableEnvironmentState::Slope:	

				//Set Stopper Flag
				if (pushable.StackLowerItem == NO_ITEM)
					SetPushableStopperFlag(true, pushableItem.Pose.Position, pushableItem.RoomNumber);

				pushable.BehaviourState = PushablePhysicState::Falling;
				pushable.Gravity = GRAVITY_AIR;

			break;

			case PushableEnvironmentState::ShallowWater:
				
				TENLog("Warning, detected unexpected shallowWater routine during FLOATING state, pushable ID: " + std::to_string(itemNumber), LogLevel::Warning, LogConfig::All, false);
				
				pushableItem.Pose.Position.y = floorHeight;
				pushable.BehaviourState = PushablePhysicState::Idle;
				pushable.Gravity = GRAVITY_AIR;
				pushableItem.Animation.Velocity.y = 0.0f;
				ActivateClimbablePushableCollider(itemNumber);

			break;

			case PushableEnvironmentState::DeepWater:
			case PushableEnvironmentState::GroundWater:

				//Calculate goal height
				if (pushable.WaterSurfaceHeight == NO_HEIGHT)
				{
					//Check if there are space for the floating effect:
					if (abs(ceilingHeight - floorHeight) >= GetPushableHeight(pushableItem) + WATER_SURFACE_DISTANCE)
					{
						//If so, put pushable to float under the ceiling.
						goalHeight = ceilingHeight + WATER_SURFACE_DISTANCE + pushable.Height;
					}
					else
					{
						//Otherwise, the pushable is "blocking all the gap", so we won't move it.
						goalHeight = pushableItem.Pose.Position.y;
					}
				}
				else
				{
					//No ceiling, so rise above water
					goalHeight = pushable.WaterSurfaceHeight - WATER_SURFACE_DISTANCE + pushable.Height;
				}

				//Manage gravity force
				pushable.Gravity = std::max(pushable.Gravity - GRAVITY_ACCEL, -GRAVITY_WATER);

				//Move pushable upwards
				if (goalHeight < pushableItem.Pose.Position.y)
				{
					// Floating up.
					pushableItem.Pose.Position.y += pushableItem.Animation.Velocity.y;
					pushableItem.Animation.Velocity.y = std::min(	pushableItem.Animation.Velocity.y + pushable.Gravity,
																	PUSHABLE_WATER_VELOCITY_MAX);
					//Fixing orientation slowly:
					PushableFallingOrientation(pushableItem);
				}
				else
				{
					//Reached goal height
					pushableItem.Pose.Position.y = goalHeight;
					pushable.BehaviourState = PushablePhysicState::WatersurfaceIdle;
					pushable.Gravity = GRAVITY_WATER;
					pushableItem.Animation.Velocity.y = 0.0f;
					pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);
					ActivateClimbablePushableCollider(itemNumber);
				}

			break;

			default:
				TENLog("Error detecting the pushable environment state during FLOATING with pushable ID: " + std::to_string(itemNumber), LogLevel::Warning, LogConfig::All, false);
			break;
		}
	}

	void HandleUnderwaterState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		int floorHeight;
		PushableEnvironmentState envState = CheckPushableEnvironment(itemNumber, floorHeight);

		switch (envState)
		{
			case PushableEnvironmentState::Ground:
			case PushableEnvironmentState::Slope:
			case PushableEnvironmentState::Air:
				//Set Stopper Flag
				if (pushable.StackLowerItem == NO_ITEM)
					SetPushableStopperFlag(true, pushableItem.Pose.Position, pushableItem.RoomNumber);

				pushableItem.Pose.Position.y = floorHeight;
				pushable.BehaviourState = PushablePhysicState::Idle;
				pushableItem.Animation.Velocity.y = 0.0f;
				pushable.Gravity = GRAVITY_AIR;	
			break;

			case PushableEnvironmentState::GroundWater:
			{
				//If shallow water, change to idle
				if (pushable.WaterSurfaceHeight != NO_HEIGHT)
				{
					int waterheight = abs(floorHeight - pushable.WaterSurfaceHeight);
					if (waterheight < GetPushableHeight(pushableItem))
					{
						pushableItem.Pose.Position.y = floorHeight;
						pushable.BehaviourState = PushablePhysicState::Idle;
						pushableItem.Animation.Velocity.y = 0.0f;
						pushable.Gravity = GRAVITY_AIR;
					}
				}
				else if (pushable.IsBuoyant && pushable.StackUpperItem == NO_ITEM)
				{
					pushable.BehaviourState = PushablePhysicState::Floating;
					pushableItem.Animation.Velocity.y = 0.0f;
					pushable.Gravity = 0.0f;
				}

				//Otherwise, remain stuck to the floor.
				pushableItem.Pose.Position.y = floorHeight;
				int heightdifference = floorHeight - pushableItem.Pose.Position.y;
				VerticalPosAddition(itemNumber, heightdifference);
			}
			break;

			case PushableEnvironmentState::ShallowWater:
			case PushableEnvironmentState::DeepWater:
				
				if (pushable.IsBuoyant && pushable.StackUpperItem == NO_ITEM)
				{
					pushable.BehaviourState = PushablePhysicState::Floating;
					pushableItem.Animation.Velocity.y = 0.0f;
					pushable.Gravity = 0.0f;
					return;
				}

				//Only pass to sinking if distance to noticeable. If is small, just stuck it to the ground.
				if (abs(floorHeight - pushableItem.Pose.Position.y) > CLICK(0.75f))
				{
					//Reset Stopper Flag
					if (pushable.StackLowerItem == NO_ITEM)
						SetPushableStopperFlag(false, pushableItem.Pose.Position, pushableItem.RoomNumber);
					
					pushable.BehaviourState = PushablePhysicState::Sinking;
					pushableItem.Animation.Velocity.y = 0.0f;
					pushable.Gravity = 0.0f;
				}
				else
				{
					pushableItem.Pose.Position.y = floorHeight;
				}

			break;
		
			default:
				TENLog("Error detecting the pushable environment state during UNDERWATER IDLE with pushable ID: " + std::to_string(itemNumber), LogLevel::Warning, LogConfig::All, false);
			break;
		}
	}

	void HandleWatersurfaceState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		int floorHeight;
		int ceilingHeight;
		PushableEnvironmentState envState = CheckPushableEnvironment(itemNumber, floorHeight, &ceilingHeight);

		switch (envState)
		{
			case PushableEnvironmentState::Ground:
			case PushableEnvironmentState::Slope:
			case PushableEnvironmentState::Air:
				pushable.BehaviourState = PushablePhysicState::Falling;
				pushableItem.Animation.Velocity.y = 0.0f;
				pushable.Gravity = GRAVITY_AIR;
				pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);
				DeactivateClimbablePushableCollider(itemNumber);
				
			break;

			case PushableEnvironmentState::ShallowWater:
			case PushableEnvironmentState::DeepWater:

				// Effects: Do water ondulation effect.
				if (!pushable.UsesRoomCollision)
					FloatingItem(pushableItem, pushable.FloatingForce);
				else
					FloatingBridge(pushableItem, pushable.FloatingForce);

				// Effects: Spawn ripples.
				DoPushableRipples(itemNumber);
			break;

			case PushableEnvironmentState::GroundWater:
			{
				//If shallow water, change to idle
				int waterheight = abs(floorHeight - pushable.WaterSurfaceHeight);
				if (waterheight < GetPushableHeight(pushableItem))
				{
					pushable.BehaviourState = PushablePhysicState::Idle;
					pushable.Gravity = GRAVITY_AIR;
				}
				else
				{
					pushable.BehaviourState = PushablePhysicState::UnderwaterIdle;
					pushable.Gravity = GRAVITY_WATER;
				}
				pushableItem.Pose.Position.y = floorHeight;
				pushableItem.Animation.Velocity.y = 0.0f;
			}
			break;

			default:
				TENLog("Error detecting the pushable environment state during WATER SURFACE IDLE with pushable ID: " + std::to_string(itemNumber), LogLevel::Warning, LogConfig::All, false);
			break;
		}
	}

	void HandleSlidingState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		//TODO:
		//1. DETECT FLOOR, (Slope orientation)
		//2. CALCULATE DIRECTION AND SPEED
		//3. MOVE OBJECT
		//4. DETECT END CONDITIONS OF NEXT SECTOR 
			//	Is a slope-> keep sliding
			//	Is a flat floor -> so ends slide
			//	Is a fall -> so passes to falling
			//	Is a forbiden Sector -> freezes the slide
		//5. Incorporate effects? Smoke or sparkles?

	}

	void HandleStackHorizontalMoveState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		auto& MovingPushableItem = g_Level.Items[Lara.Context.InteractedItem];
		pushableItem.Pose.Position.x = MovingPushableItem.Pose.Position.x;
		pushableItem.Pose.Position.z = MovingPushableItem.Pose.Position.z;
	}

	/*TODO: 
	void HandleStackFallingState(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		auto& MovingPushableItem = g_Level.Items[Lara.Context.InteractedItem];
		pushableItem.Pose.Position.y = MovingPushableItem.Pose.Position.y;
	}*/
}
