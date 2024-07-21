#include "framework.h"
#include "Scripting/Internal/TEN/Inventory/InventoryHandler.h"

#include "Game/gui.h"
#include "Game/Hud/Hud.h"
#include "Game/Lara/lara.h"
#include "Game/pickup/pickup.h"
#include "Scripting/Internal/ReservedScriptNames.h"

using namespace TEN::Hud;
using namespace TEN::Gui;

/***
Equipment manipulation.
Used to change the parameters of the player's stuff
@tentable Equipment
@pragma nostrip
*/

namespace TEN::Scripting::Equipment 
{




















	void Register(sol::state* state, sol::table& parent)
	{
		auto tableEquipment = sol::table{ state->lua_state(), sol::create };
		parent.set(ScriptReserved_Inventory, tableEquipment);

	}


};