#pragma once
#include "Game/collision/collide_room.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_test_structs.h"
#include "Game/Lara/PlayerContextData.h"

struct CollisionInfo;
struct ItemInfo;

using namespace TEN::Entities::Player;

// -----------------------------
// TEST FUNCTIONS
// For State Control & Collision
// -----------------------------

bool TestPlayerInteractAngle(const ItemInfo& item, short testAngle);

bool TestLaraClimbIdle(ItemInfo* item, CollisionInfo* coll);
bool TestLaraHangOnClimbableWall(ItemInfo* item, CollisionInfo* coll);
bool TestLaraNearClimbableWall(ItemInfo* item, FloorInfo* floor = nullptr);

bool TestLaraWall(const ItemInfo* item, float dist, float height);
bool TestLaraFacingCorner(const ItemInfo* item, short headingAngle, float dist);
bool LaraPositionOnLOS(ItemInfo* item, short angle, int distance);
int	 LaraFloorFront(ItemInfo* item, short angle, int distance);
int	 LaraCeilingFront(ItemInfo* item, short angle, int distance, int height);

CollisionResult LaraCollisionFront(ItemInfo* item, short angle, int distance);
CollisionResult LaraCeilingCollisionFront(ItemInfo* item, short angle, int distance, int height);

bool TestPlayerWaterStepOut(ItemInfo* item, CollisionInfo* coll);
bool TestLaraWaterClimbOut(ItemInfo* item, CollisionInfo* coll);
bool TestLaraLadderClimbOut(ItemInfo* item, CollisionInfo* coll);
void TestLaraWaterDepth(ItemInfo* item, CollisionInfo* coll);

bool TestLaraWeaponType(LaraWeaponType refWeaponType, const std::vector<LaraWeaponType>& weaponTypeList);
bool IsStandingWeapon(const ItemInfo* item, LaraWeaponType weaponType);
bool IsVaultState(int state);
bool IsJumpState(int state);
bool IsRunJumpQueueableState(int state);
bool IsRunJumpCountableState(int state);

bool TestLaraMoveTolerance(ItemInfo* item, CollisionInfo* coll, MoveTestSetup testSetup, bool useCrawlSetup = false);

bool TestLaraMonkeyMoveTolerance(ItemInfo* item, CollisionInfo* coll, MonkeyMoveTestSetup testSetup);
bool TestLaraMonkeyForward(ItemInfo* item, CollisionInfo* coll);
bool TestLaraMonkeyBack(ItemInfo* item, CollisionInfo* coll);
bool TestLaraMonkeyShimmyLeft(ItemInfo* item, CollisionInfo* coll);
bool TestLaraMonkeyShimmyRight(ItemInfo* item, CollisionInfo* coll);

std::optional<VaultTestResult> TestLaraLadderAutoJump(ItemInfo* item, CollisionInfo* coll);
std::optional<VaultTestResult> TestLaraLadderMount(ItemInfo* item, CollisionInfo* coll);
bool TestAndDoLaraLadderClimb(ItemInfo* item, CollisionInfo* coll);

CrawlVaultTestResult TestLaraCrawlVaultTolerance(ItemInfo* item, CollisionInfo* coll, CrawlVaultTestSetup testSetup);
CrawlVaultTestResult TestLaraCrawlUpStep(ItemInfo* item, CollisionInfo* coll);
CrawlVaultTestResult TestLaraCrawlDownStep(ItemInfo* item, CollisionInfo* coll);
CrawlVaultTestResult TestLaraCrawlExitDownStep(ItemInfo* item, CollisionInfo* coll);
CrawlVaultTestResult TestLaraCrawlExitJump(ItemInfo* item, CollisionInfo* coll);
CrawlVaultTestResult TestLaraCrawlVault(ItemInfo* item, CollisionInfo* coll);
bool TestLaraCrawlToHang(ItemInfo* item, CollisionInfo* coll);

bool TestLaraPoleCollision(ItemInfo* item, CollisionInfo* coll, bool goingUp, float offset = 0.0f);
bool TestLaraPoleUp(ItemInfo* item, CollisionInfo* coll);
bool TestLaraPoleDown(ItemInfo* item, CollisionInfo* coll);
