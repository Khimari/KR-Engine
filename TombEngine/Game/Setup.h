#pragma once

#include "Game/control/box.h"
#include "Objects/objectslist.h"
#include "Renderer/RendererEnums.h"
#include "Specific/level.h"

class Vector3i;
struct CollisionInfo;
struct ItemInfo;

constexpr auto DEFAULT_RADIUS = 10;
constexpr auto GRAVITY		  = 6.0f;
constexpr auto SWAMP_GRAVITY  = GRAVITY / 3.0f;

enum JointRotationFlags
{
	ROT_X = 1 << 2,
	ROT_Y = 1 << 3,
	ROT_Z = 1 << 4
};

// Custom LOT definition for Creature. Used in InitializeSlot() in lot.cpp.
enum class LotType
{
	Skeleton,
	Basic,
	Water,
	WaterAndLand,
	Human,
	HumanPlusJump,
	HumanPlusJumpAndMonkey,
	Flyer,
	Blockable,	  // For large creatures such as trex and shiva.
	Spider,		  // Only 2 block vault allowed.
	Ape,		  // Only 2 block vault allowed.
	SnowmobileGun // Only 1 block vault allowed and 4 block drop max.
};

enum class HitEffect
{
	None,
	Blood,
	Smoke,
	Richochet,
	NonExplosive,
	Special
};

enum class DamageMode
{
	None,
	Any,
	Explosion
};

enum class ShatterType
{
	None,
	Fragment,
	Explode
};

// MoveableAsset
struct ObjectInfo
{
	bool loaded = false; // IsLoaded

	int ID = 0;

	int nmeshes; // BoneCount, SpriteCount
	int meshIndex; // Base index in g_Level.Meshes.
	int boneIndex; // Base index in g_Level.Bones.
	int animIndex; // Base index in g_Level.Anims.
	int frameBase; // Base index in g_Level.Frames.

	LotType LotType;
	HitEffect hitEffect;
	DamageMode damageType;
	ShadowMode shadowType;

	int meshSwapSlot;
	int pivotLength;
	int radius;

	int HitPoints;
	bool intelligent;	// IsIntelligent
	bool waterCreature; // IsWaterCreature
	bool nonLot;		// IsNonLot
	bool isPickup;		// IsPickup
	bool isPuzzleHole;	// IsReceptacle
	bool usingDrawAnimatingItem;

	DWORD explodableMeshbits;

	std::function<void(short itemNumber)> Initialize = nullptr;
	std::function<void(short itemNumber)> control = nullptr;
	std::function<void(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)> collision = nullptr;

	std::function<void(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)> HitRoutine = nullptr;
	std::function<void(ItemInfo* item)> drawRoutine = nullptr;

	void SetBoneRotationFlags(int boneID, int flags);
	void SetHitEffect(HitEffect hitEffect);
	void SetHitEffect(bool isSolid = false, bool isAlive = false);
};

struct StaticAsset
{
	GAME_OBJECT_ID ID = GAME_OBJECT_ID::ID_NO_OBJECT;

	MeshData		Mesh		  = {};
	GameBoundingBox visibilityBox = {};
	GameBoundingBox collisionBox  = {};
	ShatterType		shatterType	  = ShatterType::None;
	int				shatterSound  = 0;

	int flags = 0;
};

struct SpriteAsset
{
	static const auto UV_COUNT = 4;

	int TileID = 0;
	std::array<Vector2, UV_COUNT> Uvs	 = {};
};

struct SpriteSequenceAsset
{
	int ID = 0;
	std::vector<SpriteAsset> Sprites = {};
};

class ObjectHandler
{
private:
	ObjectInfo _objects[ID_NUMBER_OBJECTS];

public:
	void Initialize();
	bool CheckID(GAME_OBJECT_ID objectID, bool isSilent = false);

	ObjectInfo& operator [](int objectID);

private:
	ObjectInfo& GetFirstAvailableObject();
};

extern ObjectHandler Objects;

// Initializers

void InitializeGameFlags();
void InitializeSpecialEffects();
void InitializeAssets();

// Inquirers

bool IsSpriteSequenceAssetLoaded(GAME_OBJECT_ID id, int spriteID = 0);

// Getters

const ObjectInfo&		   GetMoveableAsset(GAME_OBJECT_ID id);
const StaticAsset&		   GetStaticAsset(GAME_OBJECT_ID id);
const SpriteSequenceAsset& GetSpriteSequenceAsset(GAME_OBJECT_ID id);
