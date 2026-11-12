#pragma once

#include "Data/AnimalDefinition.h"
#include "Rules/WildlifeRules.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/Random.h"
#include "State/TileMap.h"
#include "State/WildlifeState.h"
#include "Views/ViewArea.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/ParticleSimulation2D.h"
#include "Hydro/Scene/Scene.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// The animals in view, per region as WildlifeRules::Shown says: walkers and birds as sprites, fish (with a sheet: shapes,
	// ripples, leaps), insects and flocks as particles; they fade in and out. Hidden species once discovered. Only a look.
	class AnimalsView
	{
	public:
		static constexpr int MaxAnimals = 400;
		static constexpr size_t MaxSpots = 160; // particle emitters
		static constexpr float SyncInterval = 0.25f; // seconds between matching the shown animals to the numbers

		// What the view needs each frame (WorldScript fills it from the session).
		struct Inputs
		{
			const WildlifeState* Wildlife = nullptr;
			const TileMap* Map = nullptr;
			const Hydrology* Water = nullptr;
			const HeightMap* Heights = nullptr; // nullptr: flat
			float LevelHeight = 0.0f;
			glm::vec3 Centre{ 0.0f };           // the map's centre in the world, its ground at Centre.z
			float DeltaTime = 0.0f;
			bool Running = true;                // false while paused: the animals stand still
			float Burrowed = 0.0f;              // the season's
			float Sight = 0.0f;                 // the talents' AnimalSight
			bool Painting = false;
			glm::vec2 Pointer{ 0.0f };          // map tiles, where the brush paints
			float BrushRadius = 0.0f;
		};

		// Made by Bind, removed by Release (call it in OnDestroy). animals: the Animals table in order (WildlifeState's rows).
		void Bind(Scene& scene, const std::vector<AnimalDefinition>& animals)
		{
			m_Scene = &scene;
			m_Animals = animals;
			m_Root = scene.CreateEntity("Animals"); // only a tag and an ID
			m_Root.AddComponent<TransformComponent>();
			m_Looks.assign(animals.size(), Look{});
			for (size_t i = 0; i < animals.size(); i++)
			{
				const AnimalDefinition& animal = animals[i];
				Ref<Texture2D> texture = animal.Sprite != 0 ? AssetManager::GetAsset<Texture2D>(animal.Sprite) : nullptr;
				if (texture == nullptr || texture->GetSpriteCount() < 2)
				{
					continue; // particles, or a sheet that isn't there: nothing to draw from
				}
				const glm::uvec2 cell = texture->GetSprite(0).Size;
				const float pixelsPerTile = animal.PixelsPerTile > 0.0f ? animal.PixelsPerTile : 32.0f;
				Look& look = m_Looks[i];
				look.Sprite = animal.Sprite;
				look.Columns = cell.x > 0 ? texture->GetWidth() / cell.x : 1u;
				look.Size = glm::vec2((float)cell.x, (float)cell.y) / pixelsPerTile;
				look.Drawn = look.Columns > 0;
			}
		}

		void Present(const Inputs& in)
		{
			if (m_Scene == nullptr || m_Root.IsValid() == false || in.Wildlife == nullptr || in.Map == nullptr || in.Water == nullptr
				|| in.Map->GetWidth() <= 0 || in.Wildlife->GetRegionCount() == 0 || in.Wildlife->Population.size() != m_Animals.size())
			{
				return;
			}
			m_SinceSync += in.DeltaTime;
			if (m_SinceSync >= SyncInterval)
			{
				m_SinceSync = 0.0f;
				Sync(in);
			}
			glm::vec2 right(1.0f, 0.0f);
			glm::vec2 up(0.0f, -1.0f);
			ViewArea::ScreenAxes(*m_Scene, right, up);
			for (size_t i = 0; i < m_Agents.size();)
			{
				Agent& agent = m_Agents[i];
				if (Live(agent, in, right, up) == false)
				{
					Hide(agent.Sprite);
					m_FreeSprites.push_back(agent.Sprite);
					m_Agents[i] = m_Agents.back();
					m_Agents.pop_back();
					continue;
				}
				Draw(agent, in);
				i++;
			}
		}

		void Release()
		{
			if (m_Scene != nullptr && m_Root.IsValid())
			{
				m_Scene->DestroyEntity(m_Root); // and the sprites and emitters under it
			}
			m_Root = {};
			m_Agents.clear();
			m_FreeSprites.clear();
			m_Spots.clear();
		}

	private:
		enum class Motion : uint8_t
		{
			Idle, // standing, grazing (birds: on the ground)
			Walk, // walking (birds: hopping)
			Run,  // running from the brush
			Fly   // birds in the air
		};

		struct Look
		{
			AssetHandle Sprite = 0;
			uint32_t Columns = 1;
			glm::vec2 Size{ 1.0f }; // a cell, in tiles
			bool Drawn = false;
		};

		struct Agent
		{
			size_t Species = 0;
			size_t Region = 0;
			Entity Sprite;
			glm::vec2 Position{ 0.0f }; // map tiles
			glm::vec2 Target{ 0.0f };
			float Z = 0.0f;             // metres: the ground's under it, eased
			float Altitude = 0.0f;      // metres above the ground (birds)
			float Timer = 0.0f;         // seconds left of standing
			float Clock = 0.0f;         // the animation's frames so far
			int Facing = 0;             // walkers: SE, SW, NE, NW; birds: left, away, toward, right
			Motion Mode = Motion::Idle;
			float Shown = 0.0f;         // 0 to 1: fading in, out when Leaving
			bool Leaving = false;
		};

		// A particle emitter showing a fish, insect or flock species in a region; fish with a sheet also ripples and leaps.
		struct Spot
		{
			size_t Species = 0;
			size_t Region = 0;
			Entity Emitter;
			Entity Ripple; // on the water
			Entity Leap;   // out of it, facing the camera
			bool Active = false;
		};

		bool IsSprite(size_t species) const
		{
			const AnimalKind kind = m_Animals[species].Kind;
			return (kind == AnimalKind::Walker || kind == AnimalKind::Bird) && m_Looks[species].Drawn;
		}

		// Matches the animals and emitters to the numbers of the regions in view; those of other regions leave.
		void Sync(const Inputs& in)
		{
			const WildlifeState& wildlife = *in.Wildlife;
			const int width = in.Map->GetWidth();
			const int height = in.Map->GetHeight();
			glm::ivec4 rect(0, 0, width - 1, height - 1);
			ViewArea::VisibleTiles(*m_Scene, width, height, in.Centre, in.Heights != nullptr ? 8 : 4, rect);
			const int rx0 = rect.x / wildlife.RegionSize;
			const int ry0 = rect.y / wildlife.RegionSize;
			const int rx1 = rect.z / wildlife.RegionSize;
			const int ry1 = rect.w / wildlife.RegionSize;
			const auto inView = [&](size_t region)
			{
				const int rx = (int)(region % (size_t)wildlife.RegionsX);
				const int ry = (int)(region / (size_t)wildlife.RegionsX);
				return rx >= rx0 && rx <= rx1 && ry >= ry0 && ry <= ry1;
			};

			const size_t regions = wildlife.GetRegionCount();
			m_Counts.assign(m_Animals.size() * regions, 0);
			for (Agent& agent : m_Agents)
			{
				agent.Leaving = agent.Leaving || inView(agent.Region) == false;
				m_Counts[agent.Species * regions + agent.Region] += agent.Leaving ? 0 : 1;
			}
			for (Spot& spot : m_Spots)
			{
				if (spot.Active && inView(spot.Region) == false)
				{
					StopSpot(spot);
				}
				m_Counts[spot.Species * regions + spot.Region] += spot.Active ? 1 : 0;
			}

			for (int ry = ry0; ry <= ry1 && ry < wildlife.RegionsY; ry++)
			{
				for (int rx = rx0; rx <= rx1 && rx < wildlife.RegionsX; rx++)
				{
					const size_t region = (size_t)ry * (size_t)wildlife.RegionsX + (size_t)rx;
					for (size_t s = 0; s < m_Animals.size(); s++)
					{
						const bool unseen = m_Animals[s].Hidden && wildlife.IsDiscovered(s) == false;
						const int want = unseen ? 0 : WildlifeRules::Shown(m_Animals[s], wildlife.Get(s, region), in.Burrowed, in.Sight);
						int& have = m_Counts[s * regions + region];
						if (IsSprite(s))
						{
							MatchAnimals(in, s, region, want, have);
						}
						else if (m_Animals[s].Kind != AnimalKind::Walker && m_Animals[s].Kind != AnimalKind::Bird)
						{
							MatchSpots(in, s, region, want, have);
						}
					}
				}
			}
		}

		void MatchAnimals(const Inputs& in, size_t species, size_t region, int want, int& have)
		{
			for (size_t i = 0; i < m_Agents.size() && have > want; i++)
			{
				Agent& agent = m_Agents[i];
				if (agent.Species == species && agent.Region == region && agent.Leaving == false)
				{
					agent.Leaving = true;
					have--;
				}
			}
			const AnimalDefinition& animal = m_Animals[species];
			for (int tries = 0; have < want && tries < 4 && (int)m_Agents.size() < MaxAnimals; tries++)
			{
				glm::ivec2 anchor(0);
				if (FindTile(in, species, region, anchor) == false)
				{
					return;
				}
				const int spread = animal.GroupMax > animal.GroupMin ? animal.GroupMax - animal.GroupMin : 0;
				const int group = (animal.GroupMin > 1 ? animal.GroupMin : 1) + (int)m_Random.Below((uint32_t)spread + 1u);
				for (int g = 0; g < group && have < want && (int)m_Agents.size() < MaxAnimals; g++)
				{
					glm::vec2 at(anchor.x + 0.5f, anchor.y + 0.5f);
					const glm::vec2 offset((m_Random.Float() - 0.5f) * 3.0f, (m_Random.Float() - 0.5f) * 3.0f);
					if (g > 0 && Standable(in, at + offset))
					{
						at += offset;
					}
					Spawn(in, species, region, at);
					have++;
				}
			}
		}

		void Spawn(const Inputs& in, size_t species, size_t region, const glm::vec2& at)
		{
			Agent agent;
			agent.Species = species;
			agent.Region = region;
			agent.Position = at;
			agent.Target = at;
			agent.Z = GroundZ(in, at);
			agent.Timer = 1.0f + m_Random.Float() * 4.0f;
			agent.Clock = m_Random.Float() * 16.0f;
			agent.Facing = (int)m_Random.Below(4);
			agent.Sprite = TakeSprite();
			SpriteRendererComponent& renderer = agent.Sprite.GetComponent<SpriteRendererComponent>();
			renderer.Texture = m_Looks[species].Sprite;
			renderer.Opacity = 0.0f;
			m_Agents.push_back(agent);
		}

		// One frame of an animal's life; false once it has faded out.
		bool Live(Agent& agent, const Inputs& in, const glm::vec2& right, const glm::vec2& up)
		{
			agent.Shown += (agent.Leaving ? -2.0f : 2.0f) * in.DeltaTime; // fades in real time, also while paused
			if (agent.Leaving && agent.Shown <= 0.0f)
			{
				return false;
			}
			agent.Shown = agent.Shown > 1.0f ? 1.0f : (agent.Shown < 0.0f ? 0.0f : agent.Shown);
			const float dt = in.Running ? in.DeltaTime : 0.0f;
			const AnimalDefinition& animal = m_Animals[agent.Species];
			const bool bird = animal.Kind == AnimalKind::Bird;
			const glm::vec2 away = agent.Position - in.Pointer;
			const float reach = animal.FleeDistance + in.BrushRadius;
			if (in.Painting && dt > 0.0f && animal.FleeDistance > 0.0f && glm::dot(away, away) < reach * reach
				&& agent.Mode != Motion::Run && agent.Mode != Motion::Fly)
			{
				const glm::vec2 direction = glm::length(away) > 0.01f ? glm::normalize(away) : glm::vec2(1.0f, 0.0f);
				agent.Target = ClampToMap(in, agent.Position + direction * (reach + (bird ? 6.0f : 3.0f)));
				agent.Mode = bird ? Motion::Fly : Motion::Run;
			}
			if (bird)
			{
				LiveBird(agent, in, animal, dt);
			}
			else
			{
				LiveWalker(agent, in, animal, dt);
			}
			if (agent.Mode != Motion::Idle)
			{
				agent.Facing = Facing(agent.Target - agent.Position, right, up, bird);
			}
			const float ground = GroundZ(in, agent.Position);
			const float ease = 4.0f * in.DeltaTime;
			agent.Z += ground > agent.Z ? std::fmin(ground - agent.Z, ease) : -std::fmin(agent.Z - ground, ease);
			const float rate = agent.Mode == Motion::Idle ? animal.IdleRate
				: (agent.Mode == Motion::Walk ? animal.WalkRate : animal.RunRate);
			agent.Clock += rate * dt;
			return true;
		}

		void LiveWalker(Agent& agent, const Inputs& in, const AnimalDefinition& animal, float dt)
		{
			if (agent.Mode == Motion::Idle)
			{
				agent.Timer -= dt;
				if (agent.Timer <= 0.0f)
				{
					const glm::vec2 offset((m_Random.Float() - 0.5f) * 8.0f, (m_Random.Float() - 0.5f) * 8.0f);
					const glm::vec2 target = ClampToMap(in, agent.Position + offset);
					const bool fits = Standable(in, target) && SuitsAt(in, agent.Species, target);
					agent.Target = target;
					agent.Mode = fits ? Motion::Walk : Motion::Idle;
					agent.Timer = fits ? 0.0f : 1.0f + m_Random.Float() * 2.0f;
				}
				return;
			}
			const float speed = agent.Mode == Motion::Run ? animal.RunSpeed : animal.WalkSpeed;
			if (Move(agent, in, speed * dt, true))
			{
				agent.Mode = Motion::Idle;
				agent.Timer = 2.0f + m_Random.Float() * 6.0f;
			}
		}

		void LiveBird(Agent& agent, const Inputs& in, const AnimalDefinition& animal, float dt)
		{
			if (agent.Mode == Motion::Idle)
			{
				agent.Altitude = std::fmax(0.0f, agent.Altitude - 2.0f * dt);
				agent.Timer -= dt;
				if (agent.Timer > 0.0f)
				{
					return;
				}
				const bool flies = m_Random.Float() < 0.35f;
				const float reach = flies ? 20.0f : 2.0f;
				const glm::vec2 offset((m_Random.Float() - 0.5f) * reach, (m_Random.Float() - 0.5f) * reach);
				const glm::vec2 target = ClampToMap(in, agent.Position + offset);
				const bool fits = SuitsAt(in, agent.Species, target) && (flies || Standable(in, target));
				agent.Target = target;
				agent.Mode = fits ? (flies ? Motion::Fly : Motion::Walk) : Motion::Idle;
				agent.Timer = 1.0f + m_Random.Float() * 4.0f;
				return;
			}
			if (agent.Mode == Motion::Walk)
			{
				if (Move(agent, in, animal.WalkSpeed * dt, true))
				{
					agent.Mode = Motion::Idle;
				}
				return;
			}
			// Flying: up to cruising height while far, down onto the target once near.
			const float left = glm::length(agent.Target - agent.Position);
			const float cruise = left > 2.0f ? FlyHeight : FlyHeight * left * 0.5f;
			agent.Altitude += (cruise > agent.Altitude ? 1.0f : -1.0f) * std::fmin(std::fabs(cruise - agent.Altitude), 2.5f * dt);
			if (Move(agent, in, animal.RunSpeed * dt, false) && agent.Altitude < 0.05f)
			{
				const bool lands = SuitsAt(in, agent.Species, agent.Position) && Standable(in, agent.Position);
				agent.Mode = lands ? Motion::Idle : Motion::Fly;
				agent.Target = lands ? agent.Target : ClampToMap(in, agent.Position + glm::vec2(m_Random.Float() - 0.5f,
					m_Random.Float() - 0.5f) * 12.0f);
				agent.Timer = 2.0f + m_Random.Float() * 6.0f;
			}
		}

		// Moves towards the target by `step` tiles; on foot only over land without too steep a step. True once there (or,
		// on foot, when the way is blocked).
		bool Move(Agent& agent, const Inputs& in, float step, bool onFoot) const
		{
			const glm::vec2 way = agent.Target - agent.Position;
			const float distance = glm::length(way);
			if (distance <= step || distance < 0.01f)
			{
				agent.Position = agent.Target;
				return true;
			}
			const glm::vec2 next = agent.Position + way / distance * step;
			if (onFoot && (Standable(in, next) == false || std::abs(Level(in, next) - Level(in, agent.Position)) > MaxClimb))
			{
				agent.Target = agent.Position;
				return true;
			}
			agent.Position = next;
			return false;
		}

		void Draw(const Agent& agent, const Inputs& in)
		{
			const AnimalDefinition& animal = m_Animals[agent.Species];
			const Look& look = m_Looks[agent.Species];
			int row = 0;
			int frames = 1;
			if (animal.Kind == AnimalKind::Bird)
			{
				const bool flying = agent.Mode == Motion::Fly;
				row = (flying ? 0 : 4) + agent.Facing;
				frames = flying ? animal.RunFrames : (agent.Mode == Motion::Walk ? animal.WalkFrames : animal.IdleFrames);
			}
			else
			{
				const int motion = agent.Mode == Motion::Idle ? 0 : (agent.Mode == Motion::Walk ? 1 : 2);
				row = motion * 4 + agent.Facing;
				frames = motion == 0 ? animal.IdleFrames : (motion == 1 ? animal.WalkFrames : animal.RunFrames);
			}
			frames = frames > 0 && frames <= (int)look.Columns ? frames : (int)look.Columns;
			const int frame = (int)agent.Clock % (frames > 0 ? frames : 1);
			const float width = (float)in.Map->GetWidth();
			const float height = (float)in.Map->GetHeight();
			Entity sprite = agent.Sprite;
			TransformComponent& transform = sprite.GetComponent<TransformComponent>();
			// Like the objects: the quad is centred on its position and stands up about its bottom edge.
			transform.Translation = in.Centre + glm::vec3(agent.Position.x - width * 0.5f,
				height * 0.5f - agent.Position.y + look.Size.y * 0.5f, agent.Z + agent.Altitude);
			transform.Scale = glm::vec3(look.Size, 1.0f);
			SpriteRendererComponent& renderer = sprite.GetComponent<SpriteRendererComponent>();
			renderer.SpriteIndex = (uint32_t)(row * (int)look.Columns + frame);
			renderer.Opacity = agent.Shown;
			renderer.CastShadows = agent.Shown >= 1.0f; // a fading cut-out keeps its whole shadow otherwise
		}

		void MatchSpots(const Inputs& in, size_t species, size_t region, int want, int& have)
		{
			for (Spot& spot : m_Spots)
			{
				if (have <= want)
				{
					break;
				}
				if (spot.Active && spot.Species == species && spot.Region == region)
				{
					StopSpot(spot);
					have--;
				}
			}
			for (int tries = 0; have < want && tries < 3; tries++)
			{
				glm::ivec2 tile(0);
				if (FindTile(in, species, region, tile) == false)
				{
					return;
				}
				if (StartSpot(in, species, region, glm::vec2(tile.x + 0.5f, tile.y + 0.5f)) == false)
				{
					return;
				}
				have++;
			}
		}

		// False when every emitter is in use.
		bool StartSpot(const Inputs& in, size_t species, size_t region, const glm::vec2& at)
		{
			Spot* spot = nullptr;
			for (Spot& free : m_Spots)
			{
				spot = free.Active == false && spot == nullptr ? &free : spot;
			}
			if (spot == nullptr && m_Spots.size() >= MaxSpots)
			{
				return false;
			}
			if (spot == nullptr)
			{
				Spot made;
				made.Emitter = MakeEmitter("Animal Particles");
				made.Ripple = MakeEmitter("Animal Ripples");
				made.Leap = MakeEmitter("Animal Leaps");
				m_Spots.push_back(made);
				spot = &m_Spots.back();
			}
			spot->Species = species;
			spot->Region = region;
			spot->Active = true;
			const AnimalDefinition& animal = m_Animals[species];
			const float above = animal.Kind == AnimalKind::Fish ? 0.02f : (animal.Kind == AnimalKind::Insect ? 0.35f : 5.0f);
			const glm::vec3 position = in.Centre
				+ glm::vec3(at.x - (float)in.Map->GetWidth() * 0.5f, (float)in.Map->GetHeight() * 0.5f - at.y, GroundZ(in, at) + above);
			spot->Emitter.GetComponent<TransformComponent>().Translation = position;
			Configure(spot->Emitter.GetComponent<ParticleEmitter2DComponent>(), animal);
			Particles2D::Play(spot->Emitter);
			const bool sheet = animal.Kind == AnimalKind::Fish && animal.Sprite != 0;
			if (sheet)
			{
				spot->Ripple.GetComponent<TransformComponent>().Translation = position + glm::vec3(0.0f, 0.0f, 0.01f);
				ConfigureRipple(spot->Ripple.GetComponent<ParticleEmitter2DComponent>(), animal);
				Particles2D::Play(spot->Ripple);
			}
			if (sheet && animal.LeapsPerMinute > 0.0f)
			{
				spot->Leap.GetComponent<TransformComponent>().Translation = position + glm::vec3(0.0f, 0.0f, 0.05f);
				ConfigureLeap(spot->Leap.GetComponent<ParticleEmitter2DComponent>(), animal);
				Particles2D::Play(spot->Leap);
			}
			return true;
		}

		Entity MakeEmitter(const std::string& name)
		{
			Entity emitter = m_Scene->CreateEntity(name);
			emitter.AddComponent<TransformComponent>();
			emitter.AddComponent<ParticleEmitter2DComponent>().PlayOnStart = false;
			m_Scene->SetParent(emitter, m_Root, false);
			return emitter;
		}

		static void StopSpot(Spot& spot)
		{
			spot.Active = false;
			Particles2D::Stop(spot.Emitter, false); // the living particles finish
			Particles2D::Stop(spot.Ripple, false);
			Particles2D::Stop(spot.Leap, false);
		}

		// Rings spreading on the water where the fish are.
		static void ConfigureRipple(ParticleEmitter2DComponent& emitter, const AnimalDefinition& animal)
		{
			emitter = ParticleEmitter2DComponent{};
			emitter.PlayOnStart = false;
			emitter.Looping = true;
			emitter.Space = ParticleSpace::Ground;
			emitter.Lighting = ParticleLighting::Lit;
			emitter.Texture = animal.Sprite;
			emitter.SpriteIndex = (uint32_t)(animal.SpriteIndex + 2);
			emitter.Shape = EmitterShape::Circle;
			emitter.ShapeSize = glm::vec2(0.4f);
			emitter.Rate = 0.35f + animal.LeapsPerMinute / 60.0f;
			emitter.Lifetime = { 1.2f, 1.8f };
			emitter.Speed = { 0.0f, 0.02f };
			emitter.StartSize = { animal.ParticleSize * 0.8f, animal.ParticleSize * 1.1f };
			emitter.EndSizeScale = 3.0f;
			emitter.StartColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.65f);
			emitter.EndColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
			emitter.MaxParticles = 6;
		}

		// A fish leaping out of the water and back: up the screen, falling, turning nose down.
		static void ConfigureLeap(ParticleEmitter2DComponent& emitter, const AnimalDefinition& animal)
		{
			emitter = ParticleEmitter2DComponent{};
			emitter.PlayOnStart = false;
			emitter.Looping = true;
			emitter.Space = ParticleSpace::View;
			emitter.Lighting = ParticleLighting::Lit;
			emitter.Texture = animal.Sprite;
			emitter.SpriteIndex = (uint32_t)(animal.SpriteIndex + 1);
			emitter.Shape = EmitterShape::Circle;
			emitter.ShapeSize = glm::vec2(0.3f);
			emitter.Rate = animal.LeapsPerMinute / 60.0f;
			emitter.Lifetime = { 0.6f, 0.75f };
			emitter.Speed = { 1.4f, 1.9f };
			emitter.Direction = 90.0f;
			emitter.Spread = 18.0f;
			emitter.Gravity = glm::vec2(0.0f, -5.5f);
			emitter.StartRotation = { 35.0f, 50.0f };
			emitter.AngularVelocity = { -150.0f, -120.0f };
			emitter.StartSize = { animal.ParticleSize * 1.5f, animal.ParticleSize * 1.9f };
			emitter.StartColor = glm::vec4(1.0f);
			emitter.EndColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f);
			emitter.MaxParticles = 2;
		}

		// The emitter's settings for the species: fish shadows drifting in the water, insects wandering low over the
		// land, a flock crossing the sky in a burst every few seconds.
		void Configure(ParticleEmitter2DComponent& emitter, const AnimalDefinition& animal)
		{
			const float size = animal.ParticleSize;
			emitter.Texture = 0; // a reused emitter may have shown a fish's sheet
			emitter.SpriteIndex = 0;
			emitter.StartRotation = { 0.0f, 0.0f };
			emitter.AngularVelocity = { 0.0f, 0.0f };
			emitter.Looping = true;
			emitter.Space = ParticleSpace::Ground;
			emitter.Lighting = ParticleLighting::Lit;
			emitter.Blend = animal.Glow > 0.0f ? ParticleBlend::Additive : ParticleBlend::Alpha;
			emitter.EmissionIntensity = animal.Glow;
			emitter.StartColor = animal.Color;
			emitter.EndColor = glm::vec4(glm::vec3(animal.Color), 0.0f);
			emitter.StartSize = { size * 0.8f, size * 1.2f };
			emitter.EndSizeScale = 1.0f;
			emitter.Shape = EmitterShape::Circle;
			emitter.Spread = 180.0f;
			emitter.Gravity = glm::vec2(0.0f);
			emitter.BurstCount = 0;
			emitter.Duration = 1.0f;
			if (animal.Kind == AnimalKind::Fish && animal.Sprite != 0)
			{
				// The fish's shape in the water, turned every way and drifting.
				emitter.Texture = animal.Sprite;
				emitter.SpriteIndex = (uint32_t)animal.SpriteIndex;
				emitter.StartRotation = { 0.0f, 360.0f };
				emitter.AngularVelocity = { -25.0f, 25.0f };
				emitter.ShapeSize = glm::vec2(0.45f);
				emitter.Rate = 0.9f;
				emitter.Lifetime = { 2.0f, 3.5f };
				emitter.Speed = { 0.08f, 0.25f };
				emitter.Turbulence = 0.25f;
				emitter.Drag = 0.0f;
				emitter.MaxParticles = 8;
			}
			else if (animal.Kind == AnimalKind::Fish)
			{
				emitter.ShapeSize = glm::vec2(0.35f);
				emitter.Rate = 0.8f;
				emitter.Lifetime = { 1.5f, 3.0f };
				emitter.Speed = { 0.1f, 0.3f };
				emitter.Turbulence = 0.3f;
				emitter.Drag = 0.0f;
				emitter.MaxParticles = 8;
				emitter.EndSizeScale = 1.2f;
			}
			else if (animal.Kind == AnimalKind::Insect)
			{
				emitter.ShapeSize = glm::vec2(1.2f);
				emitter.Rate = 1.5f;
				emitter.Lifetime = { 3.0f, 5.0f };
				emitter.Speed = { 0.2f, 0.5f };
				emitter.Turbulence = 2.5f;
				emitter.Drag = 0.5f;
				emitter.MaxParticles = 12;
			}
			else
			{
				const int spread = animal.GroupMax > animal.GroupMin ? animal.GroupMax - animal.GroupMin : 0;
				emitter.ShapeSize = glm::vec2(0.8f);
				emitter.Rate = 0.0f;
				emitter.Duration = 7.0f;
				emitter.BurstCount = (uint32_t)(animal.GroupMin + (int)m_Random.Below((uint32_t)spread + 1u));
				emitter.Direction = m_Random.Float() * 360.0f;
				emitter.Spread = 6.0f;
				emitter.Lifetime = { 6.0f, 7.0f };
				emitter.Speed = { 1.8f, 2.2f };
				emitter.Turbulence = 0.6f;
				emitter.Drag = 0.0f;
				emitter.MaxParticles = 40;
				emitter.EndColor = glm::vec4(glm::vec3(animal.Color), animal.Color.a * 0.5f);
			}
		}

		// A random tile of the region that suits the species (and, for walkers and birds, can be stood on).
		bool FindTile(const Inputs& in, size_t species, size_t region, glm::ivec2& tile)
		{
			int x0 = 0;
			int y0 = 0;
			int x1 = 0;
			int y1 = 0;
			WildlifeRules::Bounds(*in.Wildlife, region, in.Map->GetWidth(), in.Map->GetHeight(), x0, y0, x1, y1);
			const bool onFoot = m_Animals[species].Kind == AnimalKind::Walker || m_Animals[species].Kind == AnimalKind::Bird;
			for (int tries = 0; tries < 32; tries++)
			{
				const glm::ivec2 at(x0 + (int)m_Random.Below((uint32_t)(x1 - x0 + 1)), y0 + (int)m_Random.Below((uint32_t)(y1 - y0 + 1)));
				const glm::vec2 centre(at.x + 0.5f, at.y + 0.5f);
				if (SuitsAt(in, species, centre) && (onFoot == false || Standable(in, centre)))
				{
					tile = at;
					return true;
				}
			}
			return false;
		}

		bool SuitsAt(const Inputs& in, size_t species, const glm::vec2& at) const
		{
			const int x = (int)std::floor(at.x);
			const int y = (int)std::floor(at.y);
			if (in.Map->InBounds(x, y) == false)
			{
				return false;
			}
			const size_t index = (size_t)y * (size_t)in.Map->GetWidth() + (size_t)x;
			const uint8_t moisture = index < in.Water->Moisture.size() ? in.Water->Moisture[index] : (uint8_t)0;
			const uint8_t habitat = index < in.Water->Habitat.size() ? in.Water->Habitat[index] : (uint8_t)0;
			return WildlifeRules::Suits(m_Animals[species], in.Map->Get(x, y), moisture, (int)Level(in, at), *in.Water, habitat);
		}

		static bool Standable(const Inputs& in, const glm::vec2& at)
		{
			const int x = (int)std::floor(at.x);
			const int y = (int)std::floor(at.y);
			return in.Map->InBounds(x, y) && WildlifeRules::Walkable(in.Map->Get(x, y), *in.Water);
		}

		static float Level(const Inputs& in, const glm::vec2& at)
		{
			const int x = (int)std::floor(at.x);
			const int y = (int)std::floor(at.y);
			const bool raised = in.Heights != nullptr && in.Heights->GetLevels().size() == in.Map->GetTiles().size();
			return raised && in.Map->InBounds(x, y) ? (float)in.Heights->Get(x, y) : 0.0f;
		}

		static float GroundZ(const Inputs& in, const glm::vec2& at)
		{
			return Level(in, at) * in.LevelHeight;
		}

		static glm::vec2 ClampToMap(const Inputs& in, const glm::vec2& at)
		{
			const float right = (float)in.Map->GetWidth() - 0.5f;
			const float bottom = (float)in.Map->GetHeight() - 0.5f;
			return glm::vec2(std::fmin(std::fmax(at.x, 0.5f), right), std::fmin(std::fmax(at.y, 0.5f), bottom));
		}

		// The sheet's facing for a move on the map, by where it goes on screen: walkers SE, SW, NE, NW; birds left, away,
		// toward, right.
		static int Facing(const glm::vec2& move, const glm::vec2& right, const glm::vec2& up, bool bird)
		{
			const float across = glm::dot(move, right);
			const float along = glm::dot(move, up);
			if (bird)
			{
				if (std::fabs(across) >= std::fabs(along))
				{
					return across < 0.0f ? 0 : 3;
				}
				return along > 0.0f ? 1 : 2;
			}
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
			Entity sprite = m_Scene->CreateEntity("Animal");
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

		static constexpr float FlyHeight = 2.5f; // metres birds fly above the ground
		static constexpr float MaxClimb = 1.0f;  // levels a walker steps up or down at once

		Scene* m_Scene = nullptr;
		Entity m_Root;
		std::vector<AnimalDefinition> m_Animals;
		std::vector<Look> m_Looks;
		std::vector<Agent> m_Agents;
		std::vector<Entity> m_FreeSprites;
		std::vector<Spot> m_Spots;
		std::vector<int> m_Counts; // per species and region: shown (Sync)
		Random m_Random{ 0x5EEDull };
		float m_SinceSync = SyncInterval;
	};
}
