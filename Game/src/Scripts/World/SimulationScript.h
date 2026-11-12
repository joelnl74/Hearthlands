#pragma once

#include "Hydro.h"
#include "Data/AnimalDefinition.h"
#include "Data/MilestoneDefinition.h"
#include "Data/PeopleDefinition.h"
#include "Data/ObjectDefinition.h"
#include "Data/SeasonDefinition.h"
#include "Data/TerrainRuleDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/ForestRules.h"
#include "Rules/GrowthRules.h"
#include "Rules/HabitatRules.h"
#include "Rules/MountainRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/SeasonRules.h"
#include "Rules/FarmRules.h"
#include "Rules/FeudRules.h"
#include "Rules/SettlementRules.h"
#include "Rules/SettlingRules.h"
#include "Rules/SoilRules.h"
#include "Rules/TalentRules.h"
#include "Rules/WaterRules.h"
#include "Rules/WildlifeRules.h"
#include "State/HearthlandsSession.h"

#include <vector>

namespace Hydro
{
	// The world's time on the Terrain entity: the season, then the ticks due (rock, snow and rain on changed heights, water,
	// the land and its objects growing, as the season and the talents tune them); now and then brushes and milestones.
	HY_SCRIPT()
	class SimulationScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultTerrainTypes = 4392505292887402202ull; // Assets/Data/TerrainTypes.hytable
		static constexpr uint64_t DefaultTerrainRules = 9052849889970328576ull; // Assets/Data/TerrainRules.hytable
		static constexpr uint64_t DefaultObjectTypes = 9091122517145490043ull;  // Assets/Data/Objects.hytable
		static constexpr uint64_t DefaultMilestones = 5179923417704519381ull;   // Assets/Data/Milestones.hytable
		static constexpr uint64_t DefaultSeasons = 6640277151823094117ull;      // Assets/Data/Seasons.hytable
		static constexpr uint64_t DefaultAnimals = 8152222171692113247ull;      // Assets/Data/Animals.hytable
		static constexpr uint64_t DefaultPeoples = 3987286432867960408ull;      // Assets/Data/Peoples.hytable

		HY_PROPERTY(Asset(DataTable, TerrainTypeDefinition), Tooltip("Roles (water, riverbed) and unlocks"))
		AssetHandle TerrainTypes = DefaultTerrainTypes;
		HY_PROPERTY(Asset(DataTable, TerrainRuleDefinition), Tooltip("How land changes by itself"))
		AssetHandle TerrainRules = DefaultTerrainRules;
		HY_PROPERTY(Asset(DataTable, ObjectDefinition), Tooltip("What grows and stands on the tiles"))
		AssetHandle ObjectTypes = DefaultObjectTypes;
		HY_PROPERTY(Asset(DataTable, MilestoneDefinition), Tooltip("First-time events that give talent points"))
		AssetHandle Milestones = DefaultMilestones;
		HY_PROPERTY(Asset(DataTable, SeasonDefinition), Tooltip("The year: rivers, snowline, growth and colours by season"))
		AssetHandle Seasons = DefaultSeasons;
		HY_PROPERTY(Asset(DataTable, AnimalDefinition), Tooltip("The wild animals: where they live, how they grow and spread"))
		AssetHandle Animals = DefaultAnimals;
		HY_PROPERTY(Asset(DataTable, PeopleDefinition), Tooltip("The peoples: what they need, gather, hunt and build"))
		AssetHandle Peoples = DefaultPeoples;
		HY_PROPERTY(Range(1, 255), Tooltip("The centre object of a people without their own CentreObjectId (Objects table)"))
		int FireObjectId = 60;
		HY_PROPERTY(Range(1, 1024), Tooltip("Regions of the map whose animals are worked out each tick"))
		int RegionsPerTick = 16;
		HY_PROPERTY(Range(0, 10), Tooltip("Days at the end of each season over which it turns into the next"))
		float BlendDays = 1.5f;
		HY_PROPERTY(Range(1, 20), Tooltip("Ticks per world second (at 1x)"))
		float TicksPerSecond = 4.0f;
		HY_PROPERTY(Range(0, 200000), Tooltip("Random tiles the terrain rules look at per tick"))
		int SamplesPerTick = 20000;
		HY_PROPERTY(Range(0, 200000), Tooltip("Random tiles whose soil builds up or wears out per tick"))
		int SoilSamplesPerTick = 5000;
		HY_PROPERTY(Range(1, 200), Tooltip("How far a one-tile-wide channel carries water from the sea, in tiles"))
		float SourceHead = 24.0f;
		HY_PROPERTY(Range(0.1, 10), Tooltip("Head lost per tile in a narrow channel; wide ones lose less"))
		float Loss = 1.0f;
		HY_PROPERTY(Range(1, 32), Tooltip("Tiles over which moisture fades from fresh water"))
		int MoistureReach = 6;
		HY_PROPERTY(Range(1, 200), Tooltip("How far a one-tile-wide channel carries meltwater from a mountain's foot, in tiles"))
		float MeltHead = 24.0f;
		HY_PROPERTY(Range(1, 64), Tooltip("Tiles of raised land below the snow that meltwater runs down (and wets)"))
		int MeltReach = 16;
		HY_PROPERTY(Range(0, 1), Tooltip("Moisture (0 to 1) meltwater gives the land right below the snow, fading over MeltReach"))
		float MeltMoisture = 0.7f;
		HY_PROPERTY(Range(0, 0.25), Tooltip("Moisture each level gives all raised land (rain); 0: only snow and rivers wet hills"))
		float RainPerLevel = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Groundwater (0 to 1) from which a dug riverbed springs"))
		float SpringFrom = 0.78f;
		HY_PROPERTY(Range(1, 200), Tooltip("A spring's head at full groundwater (tiles of narrow channel it fills); weaker ones less"))
		float SpringHead = 20.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Moisture full groundwater gives the land above it"))
		float GroundwaterMoisture = 0.12f;
		HY_PROPERTY(Range(0.5, 9), Tooltip("Canopy around a tile (a grown tree about 1) that shades it fully"))
		float ForestCover = 3.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Share of its trees' moisture a fully shaded tile keeps (grass under and beside forest)"))
		float ForestKeep = 0.9f;

	protected:
		void OnCreate() override
		{
			const DataTableView<TerrainTypeDefinition> types = Data::GetTable<TerrainTypeDefinition>(TerrainTypes);
			for (const auto& [id, type] : types.Rows())
			{
				m_Types.push_back(type);
			}
			const DataTableView<TerrainRuleDefinition> rules = Data::GetTable<TerrainRuleDefinition>(TerrainRules);
			for (const auto& [id, rule] : rules.Rows())
			{
				m_Rules.push_back(rule);
			}
			const DataTableView<ObjectDefinition> objects = Data::GetTable<ObjectDefinition>(ObjectTypes);
			for (const auto& [id, object] : objects.Rows())
			{
				m_Objects.push_back(object);
			}
			m_ObjectsById = ObjectRules::IndexById(m_Objects);
			for (const auto& [id, people] : Data::GetTable<PeopleDefinition>(Peoples).Rows())
			{
				m_Peoples.push_back(people);
			}
			const DataTableView<MilestoneDefinition> milestones = Data::GetTable<MilestoneDefinition>(Milestones);
			for (const auto& [id, milestone] : milestones.Rows())
			{
				m_Milestones.push_back(milestone);
			}
			std::vector<SeasonDefinition> seasons;
			const DataTableView<SeasonDefinition> seasonTable = Data::GetTable<SeasonDefinition>(Seasons);
			for (const auto& [id, season] : seasonTable.Rows())
			{
				seasons.push_back(season);
			}
			m_Seasons = SeasonRules::Sorted(seasons);
			const DataTableView<AnimalDefinition> animals = Data::GetTable<AnimalDefinition>(Animals);
			for (const auto& [id, animal] : animals.Rows())
			{
				m_Animals.push_back(animal);
			}
			m_Cover = WildlifeRules::CoverOf(m_Objects);
			m_SoilCover = SoilRules::CoverOf(m_Objects);
			m_Soil = SoilRules::FromTypes(m_Types, SoilRules::Settings{});
			WaterRules::Settings settings;
			settings.SourceHead = SourceHead;
			settings.Loss = Loss;
			settings.MoistureReach = MoistureReach;
			settings.MeltHead = MeltHead;
			settings.RainPerLevel = RainPerLevel;
			settings.MeltMoisture = MeltMoisture;
			settings.Groundwater.SpringFrom = SpringFrom;
			settings.Groundwater.SpringHead = SpringHead;
			settings.Groundwater.MinSpringHead = SpringHead * 0.3f;
			settings.Groundwater.Moisture = GroundwaterMoisture;
			m_Settings = WaterRules::FromTypes(m_Types, settings);
			m_Base = m_Settings;
			MountainRules::Settings mountains;
			mountains.MeltReach = MeltReach;
			m_Mountains = MountainRules::FromTypes(m_Types, mountains);
			ForestRules::Settings forest;
			forest.FullCover = ForestCover;
			forest.Keep = ForestKeep;
			m_Forest = ForestRules::FromTypes(m_Types, forest);
		}

		void OnUpdate(float deltaTime) override
		{
			HearthlandsSession& session = HearthlandsSession::Get();
			if (session.Loading)
			{
				return; // a save was loaded: the scene starts again with it
			}
			if (session.Map.GetWidth() == 0)
			{
				return;
			}
			if (session.Heights.GetLevels().size() != session.Map.GetTiles().size())
			{
				session.Heights.Replace(HeightMap(session.Map.GetWidth(), session.Map.GetHeight())); // flat, if nothing made any
			}
			if (session.Water.Head.size() != session.Map.GetTiles().size())
			{
				MountainRules::Settle(session.Map, session.Heights, m_Mountains); // rock and snow on a generated world's hills
				session.Water = WaterRules::Start(session.Map, session.Land, m_Settings, &session.Heights, &session.Groundwater);
				session.Water.Sealed.assign(session.Map.GetTiles().size(), 0);
				for (uint32_t bed : session.DryCreeks)
				{
					if (bed < session.Water.Sealed.size())
					{
						session.Water.Sealed[bed] = 1; // dry until the player digs the creek's head
					}
				}
				WaterRules::UpdateMelt(session.Map, session.Water, MountainRules::Meltwater(session.Map, session.Heights, m_Mountains));
				session.Water.Native = HabitatRules::NativeOf(session.Map, session.Water.WaterTileId); // the player's water is the rest
				session.Water.Habitat = HabitatRules::Classify(session.Map, session.Water, session.Land, session.Heights);
				m_HabitatCounts = HabitatRules::Count(session.Water.Habitat);
				m_HabitatChanged = false;
				session.Map.TakeChanges(); // the new map is the start, not a change
				session.Heights.TakeChanges();
				session.Objects.Reset(session.Map.GetTiles().size());
				ObjectRules::Scatter(session.Objects, session.Map, session.Water, m_Objects, session.Rng, &session.Heights);
			}
			if (session.Soil.Fertility.size() != session.Map.GetTiles().size())
			{
				session.Soil = SoilRules::Start(session.Map, m_Soil);
			}
			if (session.Wildlife.Population.size() != m_Animals.size() || session.Wildlife.GetRegionCount() == 0)
			{
				session.Wildlife = WildlifeRules::Start(session.Map.GetWidth(), session.Map.GetHeight(), m_Animals.size());
				WildlifeRules::Fill(session.Wildlife, m_Animals, session.Map, session.Water, session.Heights, session.Objects, m_Cover,
					WildlifeFactors(session));
			}

			ApplyRequests(session); // even paused: a rename shows at once
			session.Season = SeasonRules::At(m_Seasons, session.Clock.Seconds / SimulationClock::SecondsPerDay, BlendDays);
			ApplyTalents(session);
			ApplySeason(session);
			const float growth = (1.0f + session.Talents.Get(TalentEffect::GrowthSpeed)) * session.Season.Growth;
			const float life = (1.0f + session.Talents.Get(TalentEffect::LifeSpeed)) * session.Season.Life;
			SimulationClock& clock = session.Clock;
			clock.Seconds += (double)deltaTime * (double)clock.Speed;
			const int run = GrowthRules::TicksDue(clock.Seconds, clock.Ticks, TicksPerSecond, MaxTicksPerFrame);
			// More looks a tick speed sprouting and spreading; visitsPerDay grows with them, so growing a stage still takes GrowDays.
			const int objectSamples = (int)((float)SamplesPerTick * life);
			const float visitsPerDay = (float)objectSamples * TicksPerSecond * (float)SimulationClock::SecondsPerDay
				/ (float)session.Map.GetTiles().size();
			for (int i = 0; i < run; i++)
			{
				const std::vector<uint32_t> raised = session.Heights.TakeChanges(); // the brush's, since the last tick
				if (raised.empty() == false)
				{
					MountainRules::SettleTiles(session.Map, session.Heights, raised, m_Mountains); // the brush settles too: a no-op then
					WaterRules::UpdateRain(session.Map, session.Water, session.Heights, raised, m_Settings);
					session.MountainsChanged = true;
				}
				GameTimings& timings = session.Timings;
				WaterRules::TickReport water;
				{
					const TimedScope timed(timings, TimedPart::Water);
					water = WaterRules::Step(session.Map, session.Water, session.Land, m_Settings);
					session.Talents.SpringsFound += water.Sprung;
					m_HabitatChanged = m_HabitatChanged || raised.empty() == false || WaterMoved(session.Map, session.Water, water);
					ObjectRules::RemoveUnfit(session.Objects, session.Map, session.Water, m_Objects, m_ObjectsById, water.Changes);
					if (session.MountainsChanged && (clock.Ticks + (uint64_t)i) % TicksPerSettle == 0)
					{
						// Meltwater looks at the whole map: at most every other tick. Its changes wake the water for the next tick.
						WaterRules::UpdateMelt(session.Map, session.Water, MountainRules::Meltwater(session.Map, session.Heights,
							m_Mountains));
						session.MountainsChanged = false;
					}
				}
				const SoilRules::Lookup soil{ &session.Soil, &m_Soil };
				ForestRules::Canopy canopy;
				canopy.Layer = &session.Objects;
				canopy.Objects = &m_Objects;
				canopy.ById = &m_ObjectsById;
				canopy.Forest = m_Forest;
				{
					const TimedScope timed(timings, TimedPart::Growth);
					GrowthRules::Step(session.Map, session.Water, session.Land, m_Rules, (int)((float)SamplesPerTick * growth),
						session.Rng, soil, canopy);
				}
				{
					const TimedScope timed(timings, TimedPart::Objects);
					ObjectRules::Step(session.Objects, session.Map, session.Water, session.Land, m_Objects, m_ObjectsById, objectSamples,
						visitsPerDay, session.Rng, &session.Heights, soil, m_Forest);
				}
				{
					const TimedScope timed(timings, TimedPart::Soil);
					SoilRules::Step(session.Soil, session.Map, session.Water, session.Objects, m_SoilCover, session.Land,
						(int)((float)SoilSamplesPerTick * growth), m_Soil, session.Rng);
				}
				{
					const TimedScope timed(timings, TimedPart::Wildlife);
					WildlifeRules::Update(session.Wildlife, m_Animals, session.Map, session.Water, session.Heights, session.Objects,
						m_Cover, RegionsPerTick, 1.0f / (TicksPerSecond * (float)SimulationClock::SecondsPerDay), WildlifeFactors(session),
						session.Rng);
				}
				if ((clock.Ticks + (uint64_t)i) % 4 == 0)
				{
					const TimedScope timed(timings, TimedPart::Unlocks);
					Unlock(session);
				}
				if ((clock.Ticks + (uint64_t)i) % TicksPerSettlements == 0)
				{
					const TimedScope timed(timings, TimedPart::Settlements);
					UpdateSettlements(session, (float)TicksPerSettlements / (TicksPerSecond * (float)SimulationClock::SecondsPerDay));
				}
				if (m_HabitatChanged && (clock.Ticks + (uint64_t)i) % TicksPerHabitat == 0)
				{
					// The habitats look at the whole map: only when the water changed, at most every few seconds.
					const TimedScope timed(timings, TimedPart::Habitat);
					session.Water.Habitat = HabitatRules::Classify(session.Map, session.Water, session.Land, session.Heights);
					m_HabitatCounts = HabitatRules::Count(session.Water.Habitat);
					m_HabitatChanged = false;
				}
				if ((clock.Ticks + (uint64_t)i) % TicksPerMilestones == 0)
				{
					const TimedScope timed(timings, TimedPart::Milestones);
					CheckMilestones(session);
				}
			}
			// Ticks beyond the cap are dropped, not owed: a slow frame slows the world instead of stalling it.
			clock.Ticks = (uint64_t)(clock.Seconds * (double)TicksPerSecond);
		}

	private:
		static constexpr int MaxTicksPerFrame = 8;
		static constexpr int TicksPerSettle = 2;     // meltwater looks at the whole map: at most every other tick
		static constexpr int TicksPerMilestones = 8; // counting looks at the whole map too: every 2 world seconds
		static constexpr int TicksPerSettlements = 4; // the settlers' day goes on every world second
		static constexpr int TicksPerHabitat = 16;    // the water's habitats, when it changed: every 4 world seconds at most

		// Whether a tick changed the water: tiles filled, dried or sprung, or a changed tile that is or was water.
		static bool WaterMoved(const TileMap& map, const Hydrology& hydrology, const WaterRules::TickReport& report)
		{
			if (report.Filled > 0 || report.Dried > 0 || report.Sprung > 0)
			{
				return true;
			}
			for (uint32_t index : report.Changes)
			{
				const bool was = index < hydrology.Habitat.size() && hydrology.Habitat[index] != 0;
				if (was || (index < map.GetTiles().size() && map.GetTiles()[index] == hydrology.WaterTileId))
				{
					return true;
				}
			}
			return false;
		}

		SettlementRules::World SettlementWorld(HearthlandsSession& session)
		{
			SettlementRules::World world;
			world.Map = &session.Map;
			world.Water = &session.Water;
			world.Objects = &session.Objects;
			world.Definitions = &m_Objects;
			world.ById = &m_ObjectsById;
			world.Land = &session.Land;
			world.Wildlife = &session.Wildlife;
			world.Animals = &m_Animals;
			world.FireObjectId = FireObjectId;
			world.Heights = &session.Heights;
			world.Soil = &session.Soil;
			world.SoilSettings = &m_Soil;
			for (const PeopleDefinition& people : m_Peoples)
			{
				world.FieldTileId = world.FieldTileId < 0 ? people.FarmTileId : world.FieldTileId;
			}
			world.Harvest = session.Season.Harvest;
			world.Growth = session.Season.Growth;
			world.Sowing = session.Season.Sowing;
			world.Frost = session.Season.Frost;
			return world;
		}

		// A rename the HUD asked for.
		void ApplyRequests(HearthlandsSession& session)
		{
			for (Settlement& settlement : session.Settlements.List)
			{
				if (settlement.Id == session.RenameRequested)
				{
					SettlementRules::Rename(settlement, m_Peoples);
					session.Settlements.Revision++;
				}
			}
			session.RenameRequested = 0;
		}

		std::string PeopleName(int row) const
		{
			return row >= 0 && row < (int)m_Peoples.size() ? m_Peoples[(size_t)row].Name : std::string("Settlers");
		}

		const Settlement* FindSettlement(const HearthlandsSession& session, int id) const
		{
			for (const Settlement& settlement : session.Settlements.List)
			{
				if (settlement.Id == id)
				{
					return &settlement;
				}
			}
			return nullptr;
		}

		// The settlers' days: new groups and branches (SettlingRules) by the talents' chances, each settlement's day, then
		// the feuds between peoples who can't stand each other; news for each.
		void UpdateSettlements(HearthlandsSession& session, float days)
		{
			const SettlementRules::World world = SettlementWorld(session);
			std::vector<float> chances(m_Peoples.size(), 0.0f);
			for (size_t row = 0; row < m_Peoples.size(); row++)
			{
				const PeopleDefinition& people = m_Peoples[row];
				const bool branch = people.EvolvesFromId > 0; // wood elves: only from elves, once Call of the woods allows it
				const float evolve = session.Talents.MayEvolve(people.PeopleId) ? 1.0f : 0.0f;
				chances[row] = branch ? evolve : session.Talents.SettleChance(people.PeopleId);
			}
			const SettlingRules::Report settling = SettlingRules::Step(session.Settlements, world, m_Peoples, chances, days, session.Rng);
			for (const SettlingRules::Arrival& arrival : settling.Arrivals)
			{
				const Settlement* made = FindSettlement(session, arrival.Id);
				const Settlement* from = FindSettlement(session, arrival.From);
				if (made != nullptr)
				{
					session.Notices.push_back(from != nullptr ? "Settlers from " + from->Name + " found " + made->Name
						: PeopleName(arrival.Row) + " have settled " + made->Name);
				}
			}
			for (const SettlingRules::Evolution& evolution : settling.Evolved)
			{
				const Settlement* changed = FindSettlement(session, evolution.Id);
				if (changed != nullptr)
				{
					session.Notices.push_back("The " + Lowered(PeopleName(evolution.From)) + " of " + changed->Name + " have become "
						+ Lowered(PeopleName(evolution.To)));
				}
			}
			for (int row : settling.NoPlace)
			{
				if (row >= 0 && row < (int)m_Peoples.size() && Counted(session, row) == 0)
				{
					session.Notices.push_back(PeopleName(row) + " find no land that suits them yet");
				}
			}

			session.FieldTileId = world.FieldTileId;
			FarmRules::Reach(session.Settlements, m_Peoples, world.FieldTileId, session.Map.GetWidth(), session.Map.GetHeight(),
				session.FarmReach);
			std::vector<Settlement>& list = session.Settlements.List;
			for (size_t i = 0; i < list.size();)
			{
				Settlement& settlement = list[i];
				const FarmRules::Work farm = FarmRules::Step(settlement, world, m_Peoples, days, session.Rng);
				if (farm.Harvested > 0 && settlement.Harvests++ == 0)
				{
					session.Notices.push_back(settlement.Name + " brings in its first harvest");
				}
				if (farm.Withered > 0 && settlement.FieldsGrowing + settlement.FieldsRipe == 0)
				{
					session.Notices.push_back("Frost took the last crops in " + settlement.Name + "'s fields");
				}
				const SettlementRules::Report report = SettlementRules::Update(settlement, world, m_Peoples, days, session.Rng);
				const bool known = settlement.People >= 0 && settlement.People < (int)m_Peoples.size();
				if (report.LeveledUp && known)
				{
					const PeopleLevel& level = SettlementRules::LevelOf(m_Peoples[(size_t)settlement.People], settlement.Level);
					session.Notices.push_back(settlement.Name + " is now a " + Lowered(level.Name)
						+ (level.Description.empty() ? std::string() : ": " + Lowered(level.Description)));
				}
				if (report.Abandoned || report.Gone)
				{
					const std::string centre = known ? m_Peoples[(size_t)settlement.People].CentreName : std::string("fire");
					session.Notices.push_back(report.Abandoned ? settlement.Name + " is empty: its people are gone"
						: settlement.Name + " is gone with its " + centre);
					if (session.SelectedSettlement == settlement.Id)
					{
						session.SelectedSettlement = 0;
					}
					SettlingRules::Remove(session.Settlements, i);
					continue;
				}
				i++;
			}

			const float calm = session.Talents.Get(TalentEffect::FeudCalm);
			for (const FeudRules::Incident& incident : FeudRules::Step(session.Settlements, world, m_Peoples, calm, days, session.Rng))
			{
				const std::string notice = Describe(session, incident);
				if (notice.empty() == false)
				{
					session.Notices.push_back(notice);
				}
			}
			session.Settlements.Revision++;
		}

		int Counted(const HearthlandsSession& session, int row) const
		{
			return SettlingRules::CountOf(session.Settlements, row);
		}

		// The news of a feud's incident: "Ironhold's dwarves raided Silverglade and took 12 food".
		std::string Describe(const HearthlandsSession& session, const FeudRules::Incident& incident) const
		{
			const Settlement* from = FindSettlement(session, incident.From);
			const Settlement* to = FindSettlement(session, incident.To);
			if (from == nullptr)
			{
				return std::string();
			}
			const std::string who = from->Name + "'s " + Lowered(PeopleName(from->People));
			const std::string them = to != nullptr ? to->Name : std::string("their neighbours");
			switch (incident.Kind)
			{
			case FeudRules::IncidentKind::Quarrel:
				return who + " and " + them + " quarrel: they live too close";
			case FeudRules::IncidentKind::Stolen:
				return who + " raided " + them + " and took " + std::to_string((int)(incident.Amount + 0.5f)) + " food";
			case FeudRules::IncidentKind::Burned:
				return who + " raided " + them + " and burned a shelter";
			case FeudRules::IncidentKind::Repelled:
				return them + "'s guards drove off raiders from " + from->Name;
			case FeudRules::IncidentKind::Fight:
				return "A fight between " + from->Name + " and " + them + ": " + std::to_string((int)incident.Amount) + " hurt";
			case FeudRules::IncidentKind::Death:
				return incident.Who + " of " + them + " was killed by " + who;
			case FeudRules::IncidentKind::Calmed:
				return from->Name + " has calmed down";
			}
			return std::string();
		}

		static std::string Lowered(std::string text)
		{
			for (char& c : text)
			{
				c = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
			}
			return text;
		}

		// The water settings the owned talents tune: wider moisture, stronger springs and springs from weaker groundwater.
		// A new moisture reach wakes every tile, so the wet ground moves to it over the next ticks.
		void ApplyTalents(HearthlandsSession& session)
		{
			const TalentState& talents = session.Talents;
			const int reach = m_Base.MoistureReach + (int)(talents.Get(TalentEffect::MoistureReach) + 0.5f);
			if (reach != m_Settings.MoistureReach)
			{
				m_Settings.MoistureReach = reach;
				for (uint32_t i = 0; i < (uint32_t)session.Water.Queued.size(); i++)
				{
					session.Water.Activate(i);
				}
			}
			m_Soil.Bonus = talents.Get(TalentEffect::SoilRichness);
			m_Forest.ShadeBare = talents.Get(TalentEffect::ForestCreep) > 0.5f;
			m_Forest.OldGrowth = talents.Get(TalentEffect::OldGrowth) > 0.5f;
			m_Settings.Groundwater.SpringHead = m_Base.Groundwater.SpringHead + talents.Get(TalentEffect::SpringHead);
			m_Settings.Groundwater.MinSpringHead = m_Settings.Groundwater.SpringHead * 0.3f;
			const float from = m_Base.Groundwater.SpringFrom - talents.Get(TalentEffect::SpringFrom);
			m_Settings.Groundwater.SpringFrom = from > 0.3f ? from : 0.3f;
		}

		// The season's water and snow, in steps (a step wakes the channels or settles the snowline again): rivers reach Flow
		// times as far (less loss a tile), meltwater runs and wets Melt times as much, the snowline moves by SnowShift.
		void ApplySeason(HearthlandsSession& session)
		{
			const float flow = SeasonRules::Step(session.Season.Flow, 20.0f);
			const float melt = SeasonRules::Step(session.Season.Melt, 20.0f);
			const float shift = SeasonRules::Step(session.Season.SnowShift, 4.0f);
			if (flow != m_Flow || melt != m_Melt)
			{
				m_Flow = flow;
				m_Melt = melt;
				m_Settings.Loss = m_Base.Loss / (flow > 0.05f ? flow : 0.05f);
				m_Settings.MeltHead = m_Base.MeltHead * melt;
				m_Settings.MeltMoisture = m_Base.MeltMoisture * melt < 1.0f ? m_Base.MeltMoisture * melt : 1.0f;
				WakeChannels(session);
			}
			if (shift != m_Mountains.SnowShift && session.Heights.GetLevels().size() == session.Map.GetTiles().size())
			{
				m_Mountains.SnowShift = shift;
				MountainRules::Settle(session.Map, session.Heights, m_Mountains); // its changes reach the water next tick
				session.MountainsChanged = true;
			}
		}

		// Wakes the water, riverbed and meltwater tiles: their heads and the moisture below the snow follow the new season.
		static void WakeChannels(HearthlandsSession& session)
		{
			const std::vector<uint8_t>& tiles = session.Map.GetTiles();
			Hydrology& water = session.Water;
			for (uint32_t i = 0; i < (uint32_t)tiles.size() && i < (uint32_t)water.Queued.size(); i++)
			{
				const bool channel = tiles[i] == water.WaterTileId || tiles[i] == water.BedTileId;
				if (channel || (i < water.Melt.size() && water.Melt[i] != 0))
				{
					water.Activate(i);
				}
			}
		}

		// The season's and the talents' hand in the animals' numbers: births, migrants here, what the land holds.
		static WildlifeRules::Factors WildlifeFactors(const HearthlandsSession& session)
		{
			WildlifeRules::Factors factors;
			factors.Life = session.Season.Life;
			factors.Migrants = session.Season.Migrants;
			factors.Capacity = 1.0f + session.Talents.Get(TalentEffect::AnimalRoom);
			return factors;
		}

		// Counts the world, keeps each milestone's progress for the talent panel and gives the points of those reached.
		void CheckMilestones(HearthlandsSession& session)
		{
			TalentRules::Counts counts = TalentRules::Count(session.Map, session.Objects, m_Objects, m_ObjectsById,
				session.Heights, session.Talents.SpringsFound);
			WildlifeRules::Discover(session.Wildlife, m_Animals); // the HUD announces them (Found)
			TalentRules::AddAnimals(counts, WildlifeRules::Totals(session.Wildlife, m_Animals), m_Animals);
			TalentRules::AddHabitats(counts, m_HabitatCounts);
			SoilRules::Qualities(session.Soil, m_Soil, m_Qualities);
			TalentRules::AddSoil(counts, m_Qualities, session.Land);
			counts.People = (uint32_t)(SettlementRules::People(session.Settlements) + 0.5f);
			for (const Settlement& settlement : session.Settlements.List)
			{
				const int id = settlement.People >= 0 && settlement.People < (int)m_Peoples.size()
					? m_Peoples[(size_t)settlement.People].PeopleId : 0;
				counts.PeopleById[(size_t)(id > 0 && id < 256 ? id : 0)] += (uint32_t)(settlement.Count + 0.5f);
				uint32_t& level = counts.LevelById[(size_t)(id > 0 && id < 256 ? id : 0)];
				level = std::max(level, (uint32_t)settlement.Level);
				counts.BestLevel = std::max(counts.BestLevel, (uint32_t)settlement.Level);
			}
			session.MilestoneProgress.resize(m_Milestones.size());
			for (size_t i = 0; i < m_Milestones.size(); i++)
			{
				session.MilestoneProgress[i] = TalentRules::Progress(m_Milestones[i], counts);
			}
			for (int milestoneId : TalentRules::Reach(session.Talents, m_Milestones, counts))
			{
				HY_INFO("Milestone reached: {}", milestoneId);
			}
		}

		void Unlock(HearthlandsSession& session) const
		{
			for (uint8_t tileId : GrowthRules::NewUnlocks(m_Types, session.UnlockedTiles, session.Map))
			{
				session.UnlockedTiles.push_back(tileId); // after the others, so the keys of the earlier brushes stay
				HY_INFO("Unlocked brush: tile {}", (int)tileId);
			}
		}

		std::vector<TerrainTypeDefinition> m_Types;
		std::vector<TerrainRuleDefinition> m_Rules;
		std::vector<ObjectDefinition> m_Objects;
		std::vector<int> m_ObjectsById;
		std::vector<MilestoneDefinition> m_Milestones;
		std::vector<SeasonDefinition> m_Seasons; // in Order
		std::vector<AnimalDefinition> m_Animals; // table order: the wildlife's rows
		std::vector<PeopleDefinition> m_Peoples; // table order: Settlement::People
		bool m_HabitatChanged = true;            // the water changed since the habitats were worked out
		std::vector<uint32_t> m_HabitatCounts;   // water tiles per WaterHabitat, as last worked out
		std::vector<uint8_t> m_Cover;            // per object id: 1 for shrubs and trees (WildlifeRules::CoverOf)
		std::vector<uint8_t> m_SoilCover;        // per object id: 1 shrubs, 2 trees (SoilRules::CoverOf)
		SoilRules::Settings m_Soil;              // from the types; the talents' richness bonus
		ForestRules::Settings m_Forest;          // the canopy's shade (ForestCover, ForestKeep)
		std::vector<uint8_t> m_Qualities;        // the soil's quality per tile, for the milestones
		float m_Flow = 1.0f;                     // the season's steps as applied
		float m_Melt = 1.0f;
		WaterRules::Settings m_Base;     // from the properties and types
		WaterRules::Settings m_Settings; // m_Base as the owned talents tune it
		MountainRules::Settings m_Mountains;
	};
}
