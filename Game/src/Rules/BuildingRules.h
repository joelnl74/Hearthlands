#pragma once

#include "Data/BuildingDefinition.h"
#include "Data/PeopleDefinition.h"
#include "State/SettlementState.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Hydro
{
	// A settlement's buildings besides its shelters (pure): known by a talent, built once needed, each lifting a cap (store,
	// meat kept, fields worked). SettlementRules finds, plans and builds them; FarmRules works the fields a barn allows.
	class BuildingRules
	{
	public:
		// What a settlement's standing buildings add up to.
		struct Bonus
		{
			float StoreDays = 0.0f;
			float HuntKeep = 0.0f;
			int Fields = 0;
		};

		// What the settlement lacks now, for deciding what to build.
		struct Needs
		{
			bool StoreFull = false;  // the store is nearly full
			bool Hunting = false;    // they hunt
			bool MoreFields = false; // there is farmland beyond what the farmers work
		};

		// The row of the building that `objectId` is for this people; -1 none.
		static int RowOf(const std::vector<BuildingDefinition>& buildings, int objectId, int peopleId)
		{
			for (size_t i = 0; i < buildings.size(); i++)
			{
				if (buildings[i].ObjectId == objectId && buildings[i].PeopleId == peopleId)
				{
					return (int)i;
				}
			}
			return -1;
		}

		static const BuildingDefinition* Find(const std::vector<BuildingDefinition>& buildings, int buildingId)
		{
			for (const BuildingDefinition& building : buildings)
			{
				if (building.BuildingId == buildingId)
				{
					return &building;
				}
			}
			return nullptr;
		}

		// A name as it reads inside a sentence ("Hunting lodge" -> "hunting lodge").
		static std::string Lower(std::string name)
		{
			if (name.empty() == false && name[0] >= 'A' && name[0] <= 'Z')
			{
				name[0] = (char)(name[0] - 'A' + 'a');
			}
			return name;
		}

		// Known: no talent needed, or it is owned.
		static bool Known(const BuildingDefinition& building, const std::vector<int>& owned)
		{
			return building.RequiresTalentId < 0
				|| std::find(owned.begin(), owned.end(), building.RequiresTalentId) != owned.end();
		}

		// This people's, known, and the settlement's level is high enough.
		static bool Allowed(const BuildingDefinition& building, const PeopleDefinition& people, const Settlement& settlement,
			const std::vector<int>& owned)
		{
			return building.PeopleId == people.PeopleId && settlement.Level >= building.MinLevel && Known(building, owned);
		}

		static bool Stands(const Settlement& settlement, int buildingId)
		{
			return std::find(settlement.Buildings.begin(), settlement.Buildings.end(), buildingId) != settlement.Buildings.end();
		}

		static Bonus Of(const std::vector<BuildingDefinition>& buildings, const Settlement& settlement)
		{
			Bonus bonus;
			for (int id : settlement.Buildings)
			{
				const BuildingDefinition* building = Find(buildings, id);
				if (building == nullptr)
				{
					continue;
				}
				bonus.StoreDays += building->StoreDays;
				bonus.HuntKeep += building->HuntKeep;
				bonus.Fields += building->Fields;
			}
			return bonus;
		}

		static bool Needed(BuildingKind kind, const Needs& needs)
		{
			switch (kind)
			{
			case BuildingKind::Store: return needs.StoreFull;
			case BuildingKind::HuntingLodge: return needs.Hunting;
			case BuildingKind::Barn: return needs.MoreFields;
			default: return false;
			}
		}

		// The building to put up next (table order): allowed, not standing yet, and needed now. Null: none.
		static const BuildingDefinition* Wanted(const std::vector<BuildingDefinition>& buildings, const PeopleDefinition& people,
			const Settlement& settlement, const std::vector<int>& owned, const Needs& needs)
		{
			for (const BuildingDefinition& building : buildings)
			{
				const bool open = Allowed(building, people, settlement, owned) && Stands(settlement, building.BuildingId) == false;
				if (open && Needed(building.Kind, needs))
				{
					return &building;
				}
			}
			return nullptr;
		}

		// What the settlement panel says about the people's buildings: standing, planned, or what is missing for each.
		struct Line
		{
			std::string Name;
			std::string State;
			bool Standing = false;
		};

		static std::vector<Line> Lines(const std::vector<BuildingDefinition>& buildings, const PeopleDefinition& people,
			const Settlement& settlement, const std::vector<int>& owned)
		{
			std::vector<Line> lines;
			for (const BuildingDefinition& building : buildings)
			{
				if (building.PeopleId != people.PeopleId)
				{
					continue;
				}
				Line line;
				line.Name = building.Name;
				line.Standing = Stands(settlement, building.BuildingId);
				if (line.Standing)
				{
					line.State = "built";
				}
				else if (Known(building, owned) == false)
				{
					line.State = "not known yet";
				}
				else if (settlement.Level < building.MinLevel)
				{
					line.State = "from level " + std::to_string(building.MinLevel + 1);
				}
				else if (settlement.Planned == building.BuildingId)
				{
					line.State = "building";
				}
				else
				{
					line.State = "when needed";
				}
				lines.push_back(line);
			}
			return lines;
		}
	};
}
