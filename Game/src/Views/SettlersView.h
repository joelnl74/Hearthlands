#pragma once

#include "Data/ObjectDefinition.h"
#include "Data/PeopleDefinition.h"
#include "Rules/SettlementRules.h"
#include "Rules/WalkRules.h"
#include "Rules/WildlifeRules.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/ObjectLayer.h"
#include "State/Random.h"
#include "State/SettlementState.h"
#include "State/TileMap.h"
#include "Views/ViewArea.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/ParticleSimulation2D.h"
#include "Hydro/Scene/Scene.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// The settlements as they look: each centre's light and smoke, and every villager at their job within the reach
	// (fishers at the shore), walking round water and steep steps (WalkRules), giving up a spot they can't get to.
	class SettlersView
	{
	public:
		static constexpr int MaxPeople = 240;

		struct Inputs
		{
			const SettlementState* Settlements = nullptr;
			const TileMap* Map = nullptr;
			const Hydrology* Water = nullptr;
			const ObjectLayer* Objects = nullptr;
			const HeightMap* Heights = nullptr;
			float LevelHeight = 0.0f;
			glm::vec3 Centre{ 0.0f }; // the map's centre in the world, its ground at Centre.z
			float DeltaTime = 0.0f;
			bool Running = true;      // false while paused: the people stand still
		};

		void Bind(Scene& scene, const std::vector<PeopleDefinition>& peoples, const std::vector<ObjectDefinition>& objects)
		{
			m_Scene = &scene;
			m_Peoples = peoples;
			m_Objects = objects;
			m_ById.assign(256, -1);
			for (size_t i = 0; i < objects.size(); i++)
			{
				if (objects[i].ObjectId > 0 && objects[i].ObjectId < 256)
				{
					m_ById[(size_t)objects[i].ObjectId] = (int)i;
				}
			}
			m_Root = scene.CreateEntity("Settlers"); // only a tag and an ID
			m_Root.AddComponent<TransformComponent>();
			m_Looks.assign(peoples.size(), Look{});
			for (size_t i = 0; i < peoples.size(); i++)
			{
				Ref<Texture2D> texture = peoples[i].Sprite != 0 ? AssetManager::GetAsset<Texture2D>(peoples[i].Sprite) : nullptr;
				if (texture == nullptr || texture->GetSpriteCount() < 2)
				{
					continue;
				}
				const glm::uvec2 cell = texture->GetSprite(0).Size;
				const float pixelsPerTile = peoples[i].PixelsPerTile > 0.0f ? peoples[i].PixelsPerTile : 32.0f;
				m_Looks[i].Sprite = peoples[i].Sprite;
				m_Looks[i].Columns = cell.x > 0 ? texture->GetWidth() / cell.x : 1u;
				m_Looks[i].Size = glm::vec2((float)cell.x, (float)cell.y) / pixelsPerTile;
				m_Looks[i].Drawn = true;
			}
		}

		void Present(const Inputs& in)
		{
			if (m_Scene == nullptr || m_Root.IsValid() == false || in.Settlements == nullptr || in.Map == nullptr
				|| in.Map->GetWidth() <= 0)
			{
				return;
			}
			SyncFires(in);
			m_SinceSync += in.DeltaTime;
			if (m_SinceSync >= 0.25f)
			{
				m_SinceSync = 0.0f;
				SyncPeople(in);
			}
			glm::vec2 right(1.0f, 0.0f);
			glm::vec2 up(0.0f, -1.0f);
			ViewArea::ScreenAxes(*m_Scene, right, up);
			for (size_t i = 0; i < m_People.size();)
			{
				Person& person = m_People[i];
				if (Live(person, in, right, up) == false)
				{
					Hide(person.Sprite);
					m_FreeSprites.push_back(person.Sprite);
					m_People[i] = m_People.back();
					m_People.pop_back();
					continue;
				}
				Draw(person, in);
				i++;
			}
		}

		void Release()
		{
			if (m_Scene != nullptr && m_Root.IsValid())
			{
				m_Scene->DestroyEntity(m_Root); // and the people and fires under it
			}
			m_Root = {};
			m_People.clear();
			m_FreeSprites.clear();
			m_Fires.clear();
		}

	private:
		enum class Doing : uint8_t
		{
			Rest, // by the fire or a shelter
			Walk,
			Work  // at a bush or tree
		};

		struct Look
		{
			AssetHandle Sprite = 0;
			uint32_t Columns = 1;
			glm::vec2 Size{ 0.5f };
			bool Drawn = false;
		};

		struct Person
		{
			int Settlement = 0;   // its Id
			int Villager = 0;     // the villager's Id in it
			size_t People = 0;    // its people's row
			Entity Sprite;
			glm::vec2 Position{ 0.0f };
			glm::vec2 Target{ 0.0f };
			bool WorkThere = false; // at the target: work (a bush or tree), else rest
			Doing Action = Doing::Rest;
			float Timer = 0.0f;
			float Clock = 0.0f;
			float Z = 0.0f;
			int Facing = 0;       // SE, SW, NE, NW
			float Shown = 0.0f;
			bool Leaving = false;
			int Side = 1;         // the side they turn to round what is in the way (WalkRules::Heading)
			float Best = 1e9f;    // the nearest they got to the target so far ...
			float Stuck = 0.0f;   // ... and the seconds since they got nearer
		};

		struct Fire
		{
			int Settlement = 0;
			Entity Flames;
			Entity Smoke;
			bool Seen = false;
		};

		// A fire and its smoke for every settlement, where its fire stands; gone settlements' fires go.
		void SyncFires(const Inputs& in)
		{
			for (Fire& fire : m_Fires)
			{
				fire.Seen = false;
			}
			for (const Settlement& settlement : in.Settlements->List)
			{
				Fire* found = nullptr;
				for (Fire& fire : m_Fires)
				{
					found = fire.Settlement == settlement.Id ? &fire : found;
				}
				if (found == nullptr)
				{
					const int row = settlement.People;
					const bool known = row >= 0 && row < (int)m_Peoples.size();
					const glm::vec3 glow = known ? m_Peoples[(size_t)row].GlowColor : glm::vec3(1.0f, 0.72f, 0.25f);
					Fire made;
					made.Settlement = settlement.Id;
					made.Flames = MakeEmitter("Fire", true, glow);
					made.Smoke = known == false || m_Peoples[(size_t)row].Smoke ? MakeEmitter("Smoke", false, glow) : Entity();
					m_Fires.push_back(made);
					found = &m_Fires.back();
				}
				found->Seen = true;
				const glm::vec2 at((float)settlement.X + 0.5f, (float)settlement.Y + 0.5f);
				const glm::vec3 position = World(in, at) + glm::vec3(0.0f, 0.0f, GroundZ(in, at));
				found->Flames.GetComponent<TransformComponent>().Translation = position + glm::vec3(0.0f, 0.0f, 0.12f);
				if (found->Smoke.IsValid())
				{
					found->Smoke.GetComponent<TransformComponent>().Translation = position + glm::vec3(0.0f, 0.0f, 0.35f);
				}
			}
			for (size_t i = 0; i < m_Fires.size();)
			{
				if (m_Fires[i].Seen == false)
				{
					m_Scene->DestroyEntity(m_Fires[i].Flames);
					if (m_Fires[i].Smoke.IsValid())
					{
						m_Scene->DestroyEntity(m_Fires[i].Smoke);
					}
					m_Fires[i] = m_Fires.back();
					m_Fires.pop_back();
					continue;
				}
				i++;
			}
		}

		// Flames: small additive sparks in `glow` rising on screen, lighting the ground; smoke: grey puffs drifting up, growing.
		Entity MakeEmitter(const char* name, bool flames, const glm::vec3& glow)
		{
			Entity entity = m_Scene->CreateEntity(name);
			entity.AddComponent<TransformComponent>();
			ParticleEmitter2DComponent& emitter = entity.AddComponent<ParticleEmitter2DComponent>();
			emitter.PlayOnStart = true;
			emitter.Looping = true;
			emitter.Space = ParticleSpace::View;
			emitter.Shape = EmitterShape::Circle;
			emitter.Direction = 90.0f;
			emitter.Lighting = ParticleLighting::Lit;
			if (flames)
			{
				emitter.ShapeSize = glm::vec2(0.12f);
				emitter.Rate = 18.0f;
				emitter.MaxParticles = 32;
				emitter.Lifetime = { 0.35f, 0.7f };
				emitter.Speed = { 0.25f, 0.5f };
				emitter.Spread = 18.0f;
				emitter.StartSize = { 0.06f, 0.11f };
				emitter.EndSizeScale = 0.3f;
				emitter.StartColor = glm::vec4(glow, 1.0f);
				const bool warm = glow.r > glow.g && glow.r > glow.b; // a fire reddens as it fades, a cold glow only dims
				emitter.EndColor = glm::vec4(glow * (warm ? glm::vec3(0.95f, 0.35f, 0.2f) : glm::vec3(0.6f)), 0.0f);
				emitter.Blend = ParticleBlend::Additive;
				emitter.EmissionIntensity = 2.0f;
				emitter.Turbulence = 0.6f;
				emitter.LightRadius = 3.0f;
				emitter.LightIntensity = 1.2f;
				emitter.MaxLights = 1;
			}
			else
			{
				emitter.ShapeSize = glm::vec2(0.1f);
				emitter.Rate = 2.5f;
				emitter.MaxParticles = 16;
				emitter.Lifetime = { 2.0f, 3.2f };
				emitter.Speed = { 0.25f, 0.4f };
				emitter.Spread = 12.0f;
				emitter.StartSize = { 0.12f, 0.18f };
				emitter.EndSizeScale = 3.0f;
				emitter.StartColor = glm::vec4(0.55f, 0.55f, 0.55f, 0.45f);
				emitter.EndColor = glm::vec4(0.7f, 0.7f, 0.72f, 0.0f);
				emitter.Blend = ParticleBlend::Alpha;
				emitter.Turbulence = 0.3f;
			}
			m_Scene->SetParent(entity, m_Root, false);
			return entity;
		}

		// A sprite for every villager (at most MaxPeople in all); those gone walk off and fade.
		void SyncPeople(const Inputs& in)
		{
			for (Person& person : m_People)
			{
				const Settlement* settlement = Find(in, person.Settlement);
				person.Leaving = person.Leaving || settlement == nullptr || FindVillager(*settlement, person.Villager) == nullptr;
			}
			for (const Settlement& settlement : in.Settlements->List)
			{
				if (settlement.People < 0 || settlement.People >= (int)m_Looks.size() || m_Looks[(size_t)settlement.People].Drawn == false)
				{
					continue;
				}
				for (const Villager& villager : settlement.Villagers)
				{
					bool drawn = false;
					for (const Person& person : m_People)
					{
						drawn = drawn || (person.Settlement == settlement.Id && person.Villager == villager.Id && person.Leaving == false);
					}
					if (drawn == false && (int)m_People.size() < MaxPeople)
					{
						Spawn(in, settlement, villager.Id);
					}
				}
			}
		}

		static const Villager* FindVillager(const Settlement& settlement, int id)
		{
			for (const Villager& villager : settlement.Villagers)
			{
				if (villager.Id == id)
				{
					return &villager;
				}
			}
			return nullptr;
		}

		const Settlement* Find(const Inputs& in, int id) const
		{
			for (const Settlement& settlement : in.Settlements->List)
			{
				if (settlement.Id == id)
				{
					return &settlement;
				}
			}
			return nullptr;
		}

		void Spawn(const Inputs& in, const Settlement& settlement, int villager)
		{
			Person person;
			person.Settlement = settlement.Id;
			person.Villager = villager;
			person.People = (size_t)settlement.People;
			const float angle = m_Random.Float() * 6.2832f;
			person.Position = glm::vec2((float)settlement.X + 0.5f + std::cos(angle) * 1.2f,
				(float)settlement.Y + 0.5f + std::sin(angle) * 1.2f);
			person.Target = person.Position;
			person.Z = GroundZ(in, person.Position);
			person.Timer = 1.0f + m_Random.Float() * 4.0f;
			person.Clock = m_Random.Float() * 8.0f;
			person.Facing = (int)m_Random.Below(4);
			person.Side = m_Random.Below(2) == 0 ? 1 : -1; // some go round an obstacle to the left, some to the right
			person.Sprite = TakeSprite();
			SpriteRendererComponent& renderer = person.Sprite.GetComponent<SpriteRendererComponent>();
			renderer.Texture = m_Looks[person.People].Sprite;
			renderer.Opacity = 0.0f;
			m_People.push_back(person);
		}

		// One frame of a person: rest or work a while, then walk to the next place (a bush or tree to work at, a shelter or
		// the fire to rest by). False once faded out.
		bool Live(Person& person, const Inputs& in, const glm::vec2& right, const glm::vec2& up)
		{
			person.Shown += (person.Leaving ? -2.0f : 2.0f) * in.DeltaTime;
			if (person.Leaving && person.Shown <= 0.0f)
			{
				return false;
			}
			person.Shown = person.Shown > 1.0f ? 1.0f : (person.Shown < 0.0f ? 0.0f : person.Shown);
			const float dt = in.Running ? in.DeltaTime : 0.0f;
			const Settlement* settlement = Find(in, person.Settlement);
			const PeopleDefinition& people = m_Peoples[person.People];
			if (person.Action == Doing::Walk)
			{
				const glm::vec2 way = person.Target - person.Position;
				const float distance = glm::length(way);
				const float step = people.WalkSpeed * dt;
				if (distance <= step || distance < 0.01f)
				{
					person.Position = person.Target;
					person.Action = person.WorkThere ? Doing::Work : Doing::Rest;
					person.Timer = 3.0f + m_Random.Float() * 5.0f;
				}
				else if (dt > 0.0f)
				{
					const glm::vec2 heading = WalkRules::Heading(GroundFor(in, people), person.Position, person.Target, person.Side);
					if (heading.x != 0.0f || heading.y != 0.0f)
					{
						person.Position += heading * step;
						person.Facing = Facing(heading, right, up);
					}
					person.Stuck = distance < person.Best - 0.3f ? 0.0f : person.Stuck + dt;
					person.Best = std::fmin(person.Best, distance);
					if (person.Stuck > GiveUpAfter)
					{
						GiveUp(person, in, settlement, people);
					}
				}
			}
			else
			{
				person.Timer -= dt;
				if (person.Timer <= 0.0f && settlement != nullptr && person.Leaving == false)
				{
					PickTarget(person, in, *settlement, people);
				}
			}
			const float ground = GroundZ(in, person.Position);
			const float ease = 4.0f * in.DeltaTime;
			person.Z += ground > person.Z ? std::fmin(ground - person.Z, ease) : -std::fmin(person.Z - ground, ease);
			const float rate = person.Action == Doing::Walk ? 7.0f : (person.Action == Doing::Work ? 3.0f : 1.5f);
			person.Clock += rate * dt;
			return true;
		}

		// The next place, by the villager's job: a bush (gatherer) or a tree (woodcutter) to work at, somewhere out in the
		// land (hunter), a shelter to work on (builder), a watch post round the centre (guard); else rest by the centre.
		void PickTarget(Person& person, const Inputs& in, const Settlement& settlement, const PeopleDefinition& people)
		{
			const Villager* villager = FindVillager(settlement, person.Villager);
			const VillagerJob job = villager != nullptr ? villager->Job : VillagerJob::Resting;
			const glm::vec2 centre((float)settlement.X + 0.5f, (float)settlement.Y + 0.5f);
			const bool working = job != VillagerJob::Resting && job != VillagerJob::Hurt && m_Random.Float() < 0.8f;
			const int area = SettlementRules::Reach(people, settlement.Level);
			const int home = SettlementRules::HomeReach(people, settlement.Level);
			if (working && (job == VillagerJob::Gatherer || job == VillagerJob::Woodcutter || job == VillagerJob::Builder))
			{
				const int reach = job == VillagerJob::Builder ? home + BuildingsBeyond : area;
				for (int tries = 0; tries < 32; tries++)
				{
					const int dx = (int)m_Random.Below((uint32_t)(reach * 2 + 1)) - reach;
					const int dy = (int)m_Random.Below((uint32_t)(reach * 2 + 1)) - reach;
					const int x = settlement.X + dx;
					const int y = settlement.Y + dy;
					if (dx * dx + dy * dy > reach * reach || in.Map->InBounds(x, y) == false)
					{
						continue;
					}
					const ObjectDefinition* object = Object(in.Objects->Get((size_t)y * (size_t)in.Map->GetWidth() + (size_t)x));
					const bool bush = object != nullptr && object->Food > 0.0f && object->Kind != ObjectKind::Tree;
					const bool tree = object != nullptr && object->Kind == ObjectKind::Tree && object->Wood > 0.0f;
					const bool building = object != nullptr && object->Kind == ObjectKind::Building;
					const bool fits = job == VillagerJob::Gatherer ? bush : (job == VillagerJob::Woodcutter ? tree : building);
					if (fits)
					{
						// Stand in front of it (the side towards the centre), on land.
						const glm::vec2 at((float)x + 0.5f, (float)y + 0.5f);
						const glm::vec2 spot = at + glm::normalize(centre - at + glm::vec2(1e-3f)) * 0.7f;
						if (Standable(in, spot))
						{
							Go(person, spot, true);
							return;
						}
					}
				}
			}
			if (working && job == VillagerJob::Farmer && people.FarmTileId >= 0)
			{
				// Out in the fields: a farmland tile within reach.
				for (int tries = 0; tries < 32; tries++)
				{
					const int dx = (int)m_Random.Below((uint32_t)(area * 2 + 1)) - area;
					const int dy = (int)m_Random.Below((uint32_t)(area * 2 + 1)) - area;
					const int x = settlement.X + dx;
					const int y = settlement.Y + dy;
					if (in.Map->InBounds(x, y) && in.Map->Get(x, y) == (uint8_t)people.FarmTileId)
					{
						Go(person, glm::vec2((float)x + 0.5f, (float)y + 0.5f), true);
						return;
					}
				}
			}
			if (working && job == VillagerJob::Fisher && in.Water != nullptr)
			{
				// At the shore: a tile beside water within reach, sitting facing it.
				for (int tries = 0; tries < 48; tries++)
				{
					const int dx = (int)m_Random.Below((uint32_t)(area * 2 + 1)) - area;
					const int dy = (int)m_Random.Below((uint32_t)(area * 2 + 1)) - area;
					const glm::vec2 spot((float)(settlement.X + dx) + 0.5f, (float)(settlement.Y + dy) + 0.5f);
					if (dx * dx + dy * dy <= area * area && Standable(in, spot) && BesideWater(in, settlement.X + dx, settlement.Y + dy))
					{
						Go(person, spot, true);
						return;
					}
				}
			}
			if (working && (job == VillagerJob::Hunter || job == VillagerJob::Forester))
			{
				const float angle = m_Random.Float() * 6.2832f;
				const float outward = job == VillagerJob::Hunter ? 0.5f : 0.35f; // foresters plant nearer
				const float reach = (float)std::min(area, FarthestOut) * (outward + 0.5f * m_Random.Float());
				const glm::vec2 spot = centre + glm::vec2(std::cos(angle), std::sin(angle)) * reach;
				if (Standable(in, spot))
				{
					Go(person, spot, true);
					return;
				}
			}
			const bool guard = job == VillagerJob::Guard;
			const float angle = m_Random.Float() * 6.2832f;
			const float reach = guard ? 3.0f + m_Random.Float() * 1.5f : 0.9f + m_Random.Float() * 2.5f;
			const glm::vec2 spot = centre + glm::vec2(std::cos(angle), std::sin(angle)) * reach;
			if (Standable(in, spot))
			{
				Go(person, spot, false);
				return;
			}
			person.Timer = 1.0f;
		}

		static void Go(Person& person, const glm::vec2& spot, bool work)
		{
			person.Target = spot;
			person.WorkThere = work;
			person.Action = Doing::Walk;
			person.Best = 1e9f;
			person.Stuck = 0.0f;
		}

		// They can't get to their spot (water or a cliff in the way): a work spot they give up and go home; on the way home
		// near it they rest where they are; far out they are lost and fade, to come back by the centre (SyncPeople).
		void GiveUp(Person& person, const Inputs& in, const Settlement* settlement, const PeopleDefinition& people)
		{
			if (settlement == nullptr)
			{
				person.Leaving = true;
				return;
			}
			const glm::vec2 centre((float)settlement->X + 0.5f, (float)settlement->Y + 0.5f);
			if (person.WorkThere)
			{
				const float angle = m_Random.Float() * 6.2832f;
				const glm::vec2 spot = centre + glm::vec2(std::cos(angle), std::sin(angle)) * (0.9f + m_Random.Float() * 2.5f);
				Go(person, Standable(in, spot) ? spot : centre, false);
				return;
			}
			const float lost = (float)(SettlementRules::HomeReach(people, settlement->Level) + 3);
			if (glm::length(person.Position - centre) > lost)
			{
				person.Leaving = true;
				return;
			}
			person.Action = Doing::Rest;
			person.Timer = 1.0f + m_Random.Float() * 2.0f;
		}

		static bool BesideWater(const Inputs& in, int x, int y)
		{
			const int dx[] = { 1, -1, 0, 0 };
			const int dy[] = { 0, 0, 1, -1 };
			for (int i = 0; i < 4; i++)
			{
				if (in.Map->InBounds(x + dx[i], y + dy[i]) && in.Map->Get(x + dx[i], y + dy[i]) == in.Water->WaterTileId)
				{
					return true;
				}
			}
			return false;
		}

		static WalkRules::Ground GroundFor(const Inputs& in, const PeopleDefinition& people)
		{
			WalkRules::Ground ground;
			ground.Map = in.Map;
			ground.Water = in.Water;
			ground.Heights = in.Heights;
			ground.Climb = people.Climb;
			return ground;
		}

		void Draw(const Person& person, const Inputs& in)
		{
			const PeopleDefinition& people = m_Peoples[person.People];
			const Look& look = m_Looks[person.People];
			const int motion = person.Action == Doing::Rest ? 0 : (person.Action == Doing::Walk ? 1 : 2);
			int frames = motion == 0 ? people.IdleFrames : (motion == 1 ? people.WalkFrames : people.WorkFrames);
			frames = frames > 0 && frames <= (int)look.Columns ? frames : (int)look.Columns;
			const int frame = (int)person.Clock % (frames > 0 ? frames : 1);
			Entity sprite = person.Sprite;
			TransformComponent& transform = sprite.GetComponent<TransformComponent>();
			// Like the objects: the quad is centred on its position and stands up about its bottom edge.
			transform.Translation = World(in, person.Position) + glm::vec3(0.0f, look.Size.y * 0.5f, person.Z);
			transform.Scale = glm::vec3(look.Size, 1.0f);
			SpriteRendererComponent& renderer = sprite.GetComponent<SpriteRendererComponent>();
			renderer.SpriteIndex = (uint32_t)((motion * 4 + person.Facing) * (int)look.Columns + frame);
			renderer.Opacity = person.Shown;
			renderer.CastShadows = person.Shown >= 1.0f; // a fading cut-out keeps its whole shadow otherwise
		}

		const ObjectDefinition* Object(uint8_t id) const
		{
			const int row = id != 0 ? m_ById[id] : -1;
			return row >= 0 ? &m_Objects[(size_t)row] : nullptr;
		}

		static glm::vec3 World(const Inputs& in, const glm::vec2& at)
		{
			return in.Centre + glm::vec3(at.x - (float)in.Map->GetWidth() * 0.5f, (float)in.Map->GetHeight() * 0.5f - at.y, 0.0f);
		}

		static bool Standable(const Inputs& in, const glm::vec2& at)
		{
			WalkRules::Ground ground;
			ground.Map = in.Map;
			ground.Water = in.Water;
			return WalkRules::Standable(ground, (int)std::floor(at.x), (int)std::floor(at.y));
		}

		static float GroundZ(const Inputs& in, const glm::vec2& at)
		{
			const int x = (int)std::floor(at.x);
			const int y = (int)std::floor(at.y);
			const bool raised = in.Heights != nullptr && in.Heights->GetLevels().size() == in.Map->GetTiles().size();
			return raised && in.Map->InBounds(x, y) ? (float)in.Heights->Get(x, y) * in.LevelHeight : 0.0f;
		}

		// SE, SW, NE, NW by where a move goes on screen.
		static int Facing(const glm::vec2& move, const glm::vec2& right, const glm::vec2& up)
		{
			const float across = glm::dot(move, right);
			const float along = glm::dot(move, up);
			if (along < 0.0f)
			{
				return across >= 0.0f ? 0 : 1;
			}
			return across >= 0.0f ? 2 : 3;
		}

		Entity TakeSprite()
		{
			if (m_FreeSprites.empty() == false)
			{
				Entity sprite = m_FreeSprites.back();
				m_FreeSprites.pop_back();
				return sprite;
			}
			Entity sprite = m_Scene->CreateEntity("Settler");
			sprite.AddComponent<TransformComponent>();
			SpriteRendererComponent& renderer = sprite.AddComponent<SpriteRendererComponent>();
			renderer.Facing = SpriteFacing::Upright;
			m_Scene->SetParent(sprite, m_Root, false);
			return sprite;
		}

		static void Hide(Entity sprite)
		{
			SpriteRendererComponent& renderer = sprite.GetComponent<SpriteRendererComponent>();
			renderer.Opacity = 0.0f;
			renderer.CastShadows = false; // a hidden cut-out sprite keeps its shadow otherwise
		}

		Scene* m_Scene = nullptr;
		Entity m_Root;
		std::vector<PeopleDefinition> m_Peoples;
		std::vector<ObjectDefinition> m_Objects;
		std::vector<int> m_ById;
		std::vector<Look> m_Looks;
		std::vector<Person> m_People;
		std::vector<Entity> m_FreeSprites;
		std::vector<Fire> m_Fires;
		Random m_Random{ 0xF14Eull };
		float m_SinceSync = 1.0f;

		static constexpr float GiveUpAfter = 3.0f;  // seconds without getting nearer before they give up a spot
		static constexpr int BuildingsBeyond = 4;   // builders go this much beyond the shelters (lodges stand further out)
		static constexpr int FarthestOut = 20;      // tiles hunters and foresters go out at most
	};
}
