#include <stdio.h>

#include "CC_API/Chat.h"
#include "CC_API/World.h"
#include "CC_API/Inventory.h"

#include "Draw.h"
#include "MarkSelection.h"
#include "MemoryAllocation.h"
#include "Messaging.h"
#include "Vectors.h"
#include "ParsingUtils.h"
#include "Array.h"
#include "SPCCommand.h"
#include "List.h"
#include "WorldUtils.h"
#include "BinaryMap.h"
#include "IVec3FastQueue.h"

#define MAX_NEIGHBORS 6

typedef enum FillMode_ {
	MODE_3D = 0,
	MODE_2DX = 1,
	MODE_2DY = 2,
	MODE_2DZ = 3,
} FillMode;

static FillMode s_Mode;

static bool TryExpand(IVec3FastQueue* queue, IVec3 target, BlockID filledOverBlock, BinaryMap* map);
static void Fill_Command(const cc_string* args, int argsCount);
static bool TryParseArguments(const cc_string* args, int argsCount);
static void ShowUsage();
static void FillSelectionHandler(IVec3* marks, int count);

static struct ChatCommand FillCommand = {
	"Fill",
	Fill_Command,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&b/Fill [mode] [brush/block]",
        "&fFills the specified area.",
		"&fList of modes: &b3d&f (default), &b2d-x&f, &b2d-y&f, &b2d-z&f.",
		NULL,
		NULL
	},
	NULL
};

SPCCommand FillSPCCommand = {
	.chatCommand = &FillCommand,
	.canStatic = true
};

static void ShowUsage() {
	Message_Player("Usage: &b/Fill [mode] [brush/block]&f.");
}

static bool TryParseArguments(const cc_string* args, int argsCount) {
    cc_string modesString[] = {
        String_FromConst("3d"),
        String_FromConst("2d-x"),
        String_FromConst("2d-y"),
        String_FromConst("2d-z"),
    };

	size_t modesCount = sizeof(modesString) / sizeof(modesString[0]);

	bool hasBlockOrBrush = (argsCount >= 2) ||
		((argsCount == 1) && (!Array_ContainsString(&args[0], modesString, modesCount)));
	bool hasMode = (argsCount >= 1) && Array_ContainsString(&args[0], modesString, modesCount);

	if (hasMode) {
		s_Mode = Array_IndexOfStringCaseless(&args[0], modesString, modesCount);
	} else {
		s_Mode = MODE_3D;
	}

	if (hasMode && s_Mode == -1) {
		Message_ShowUnknownMode(&args[0]);
		Message_ShowAvailableModes(modesString, modesCount);
		return false;
	}

	if (hasBlockOrBrush) {
		int brushIndex;

		if (hasMode) {
			brushIndex = 1;
		} else {
			brushIndex = 0;
		}

		// Checks that there are no trailing blocks in the command, e.g. `/Z Stone Air` doesn't make sense.
		bool isBlock = args[brushIndex].buffer[0] != '@';
		if (isBlock && argsCount > (brushIndex + 1)) {
			ShowUsage();
			return false;
		}

		if (!Parse_TryParseBlockOrBrush(&args[brushIndex], argsCount - brushIndex)) {
			return false;
		}

		return true;
	} else {
		Brush_LoadInventory();
		return true;
	}
}

static bool TryExpand(IVec3FastQueue* queue, IVec3 target, BlockID filledOverBlock, BinaryMap* binaryMap) {
	IVec3 neighbors[MAX_NEIGHBORS];
	short count = 0;

	if (s_Mode == MODE_3D || s_Mode == MODE_2DY || s_Mode == MODE_2DZ) {
		neighbors[count].X = target.X - 1; neighbors[count].Y = target.Y; neighbors[count].Z = target.Z;
		count++;
		neighbors[count].X = target.X + 1; neighbors[count].Y = target.Y; neighbors[count].Z = target.Z;
		count++;
	}
	
	if (s_Mode == MODE_3D || s_Mode == MODE_2DX || s_Mode == MODE_2DZ) {
		neighbors[count].X = target.X; neighbors[count].Y = target.Y - 1; neighbors[count].Z = target.Z;
		count++;
		neighbors[count].X = target.X; neighbors[count].Y = target.Y + 1; neighbors[count].Z = target.Z;
		count++;
	}
	
	if (s_Mode == MODE_3D || s_Mode == MODE_2DX || s_Mode == MODE_2DY) {
		neighbors[count].X = target.X; neighbors[count].Y = target.Y; neighbors[count].Z = target.Z - 1;
		count++;
		neighbors[count].X = target.X; neighbors[count].Y = target.Y; neighbors[count].Z = target.Z + 1;
		count++;
	}

	IVec3 neighbor;

	for (int i = 0; i < count; i++) {
		neighbor = neighbors[i];

		if (!IsInWorldBoundaries(neighbor.X, neighbor.Y, neighbor.Z) ||
			GetBlock(neighbor.X, neighbor.Y, neighbor.Z) != filledOverBlock ||
			BinaryMap_Get(binaryMap, neighbor.X, neighbor.Y, neighbor.Z)) {
			continue;
		}

		if (!IVec3FastQueue_TryEnqueue(queue, neighbor)) {
			return false;
		}

		BinaryMap_Set(binaryMap, neighbor.X, neighbor.Y, neighbor.Z);
	}

	return true;
}

static void FillSelectionHandler(IVec3* marks, int count) {
	IVec3 fillOrigin = marks[0];
	BlockID filledOverBlock = GetBlock(fillOrigin.X, fillOrigin.Y, fillOrigin.Z);
	BinaryMap* binaryMap = BinaryMap_CreateEmpty(World.Width, World.Height, World.Length);
	IVec3FastQueue* queue = IVec3FastQueue_CreateEmpty();

	BinaryMap_Set(binaryMap, fillOrigin.X, fillOrigin.Y, fillOrigin.Z);
	IVec3FastQueue_TryEnqueue(queue, fillOrigin);

	IVec3 current;

	while (!IVec3FastQueue_IsEmpty(queue)) {
		current = IVec3FastQueue_Dequeue(queue);
		TryExpand(queue, current, filledOverBlock, binaryMap);
	}

	Draw_Start("Fill");

	for (int x = 0; x < World.Width; x++) {
		for (int y = 0; y < World.Height; y++) {
			for (int z = 0; z < World.Length; z++) {
				if (BinaryMap_Get(binaryMap, x, y, z)) {
					Draw_Brush(x, y, z);
				}
			}
		}
	}

	int blocksAffected = Draw_End();
	Message_BlocksAffected(blocksAffected);

	BinaryMap_Free(binaryMap);
	IVec3FastQueue_Free(queue);
}

static void Fill_Command(const cc_string* args, int argsCount) {
	if (!TryParseArguments(args, argsCount)) {
		MarkSelection_Abort();
		return;
	}

    MarkSelection_Make(FillSelectionHandler, 1);
    Message_Player("&fPlace or break a block.");
}
