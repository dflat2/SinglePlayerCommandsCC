#include "CC_API/Chat.h"
#include "CC_API/Entity.h"
#include "CC_API/Block.h"
#include "CC_API/World.h"

#include "Messaging.h"
#include "SPCCommand.h"
#include "WorldUtils.h"

static void Ascend_Command(const cc_string* args, int argsCount);
static void Descend_Command(const cc_string* args, int argsCount);
static bool TryFindAbove(Vec3 currentPosition, Vec3* ascendPosition);
static bool TryFindBelow(Vec3 currentPosition, Vec3* descendPosition);
static bool CanStandOnBlock(int x, int y, int z);
static bool IsSolidBlock(BlockID id);
static bool CanPassThrough(BlockID id);

static struct ChatCommand AscendCommand = {
	"Ascend",
	Ascend_Command,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&b/Ascend",
		"&fTeleports you to the first free space above you.",
		NULL,
		NULL,
		NULL
	},
	NULL
};

SPCCommand AscendSPCCommand = {
	.chatCommand = &AscendCommand,
	.canStatic = false,
};

static struct ChatCommand DescendCommand = {
	"Descend",
	Descend_Command,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&b/Descend",
		"&fTeleports you to the first free space below you.",
		NULL,
		NULL,
		NULL
	},
	NULL
};

SPCCommand DescendSPCCommand = {
	.chatCommand = &DescendCommand,
	.canStatic = false,
};

static void Ascend_Command(const cc_string* args, int argsCount) {
	if (argsCount >= 1) {
		Message_Player("&fUsage: &b/Ascend");
		return;
	}

	struct LocalPlayer* player = (struct LocalPlayer*)Entities.List[ENTITIES_SELF_ID];
	struct Entity playerEntity = player->Base;

	Vec3 currentPosition = playerEntity.Position;
	Vec3 ascendPosition;

	bool success = TryFindAbove(currentPosition, &ascendPosition);

	if (!success) {
		Message_Player("&fThere are no blocks above to ascend to.");
		return;
	}

	struct LocationUpdate update;
	update.flags = LU_HAS_POS;
	update.pos = ascendPosition;

	playerEntity.VTABLE->SetLocation(&playerEntity, &update);
	Message_Player("&fTeleported you up.");
}

static void Descend_Command(const cc_string* args, int argsCount) {
	if (argsCount >= 1) {
		Message_Player("&fUsage: &b/Descend");
		return;
	}

	struct LocalPlayer* player = (struct LocalPlayer*)Entities.List[ENTITIES_SELF_ID];
	struct Entity playerEntity = player->Base;

	Vec3 currentPosition = playerEntity.Position;
	Vec3 descendPosition;

	bool success = TryFindBelow(currentPosition, &descendPosition);

	if (!success) {
		Message_Player("&fThere are no blocks below to descend to.");
		return;
	}

	struct LocationUpdate update;
	update.flags = LU_HAS_POS;
	update.pos = descendPosition;

	playerEntity.VTABLE->SetLocation(&playerEntity, &update);
	Message_Player("&fTeleported you down.");
}

static bool TryFindAbove(Vec3 currentPosition, Vec3* ascendPosition) {
	int x = (int) currentPosition.X;
	int y = (int) currentPosition.Y;
	int z = (int) currentPosition.Z;

	ascendPosition->X = currentPosition.X;
	ascendPosition->Z = currentPosition.Z;

	for (int yCandidate = y + 1; yCandidate <= World.Height; yCandidate++) {
		if (CanStandOnBlock(x, yCandidate, z)) {
			ascendPosition->Y = (float)yCandidate;
			return true;
		}
	}

	return false;
}

static bool TryFindBelow(Vec3 currentPosition, Vec3* descendPosition) {
	int x = (int) currentPosition.X;
	int y = (int) currentPosition.Y;
	int z = (int) currentPosition.Z;

	descendPosition->X = currentPosition.X;
	descendPosition->Z = currentPosition.Z;

	for (int yCandidate = y - 1; yCandidate >= 0; yCandidate--) {
		if (CanStandOnBlock(x, yCandidate, z)) {
			descendPosition->Y = (float)yCandidate;
			return true;
		}
	}

	return false;
}

static bool CanPassThrough(BlockID id) {
	enum CollideType collision = Blocks.Collide[id];
	return (collision == COLLIDE_NONE) ||
		   (collision == COLLIDE_LIQUID) ||
		   (collision == COLLIDE_WATER) ||
		   (collision == COLLIDE_LAVA) ||
		   (collision == COLLIDE_CLIMB);
}

static bool IsSolidBlock(BlockID id) {
	return !CanPassThrough(id);
}

static bool CanStandOnBlock(int x, int y, int z) {
	BlockID below = IsInWorldBoundaries(x, y - 1, z) ? GetBlock(x, y - 1, z) : BLOCK_AIR;
	BlockID feet = IsInWorldBoundaries(x, y, z) ? GetBlock(x, y, z) : BLOCK_AIR;
	BlockID head = IsInWorldBoundaries(x, y + 1, z) ? GetBlock(x, y + 1, z) : BLOCK_AIR;

	// y == 0 is always solid because it's bedrock, though outside of the world boundaries.
	if (y == 0) {
		return CanPassThrough(feet) && CanPassThrough(head);
	}

	return IsSolidBlock(below) && CanPassThrough(feet) && CanPassThrough(head);
}
