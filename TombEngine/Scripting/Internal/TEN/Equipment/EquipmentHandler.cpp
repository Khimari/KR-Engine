#include "framework.h"
#include "Scripting/Internal/TEN/Equipment/EquipmentHandler.h"

#include "Game/gui.h"
#include "Game/Hud/Hud.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_flare.h"
#include "Game/pickup/pickup.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Objects\game_object_ids.h"

using namespace TEN::Hud;
using namespace TEN::Gui;

/***
Equipment manipulation.
Used to change the parameters of the player's equipment
@tentable Equipment
@pragma nostrip
*/

namespace TEN::Scripting::Equipment 
{

	/// Set medipack health restoration value.
	//@function SetMedipackHealth
	//@tparam Objects.ObjID objectID Choose either Small Medipack or Large Medipack.
	//@tparam float value to restore, in percent of a full health bar.
	static void SetMedipackHealth(GAME_OBJECT_ID objectID, int health)
	{

		if (objectID != ID_SMALLMEDI_ITEM || ID_BIGMEDI_ITEM)
			TENLog("Cannot perform operation as no medipack items have been selected. Please use ID_SMALLMEDI_ITEM or ID_BIGMEDI_ITEM");
	};
















void Register(sol::state* state, sol::table& parent)
{
	auto tableEquipment = sol::table{ state->lua_state(), sol::create };
	parent.set(ScriptReserved_Inventory, tableEquipment);

	tableEquipment.set_function(ScriptReserved_SetMedHealth, &SetMedipackHealth);

}


};