#include "GameRegistration.h"

#include "Generated/GeneratedDataReflection.h"
#include "Generated/GeneratedScriptReflection.h"
#include "State/HearthlandsSession.h"

#include "Hydro/Project/Project.h"

namespace Hydro
{
	void RegisterGame()
	{
		// The stable id is what projects store; the name can change.
		Project::SetCompiledGame({ "hearthlands", "Hearthlands" });

		// Data types first: scripts' data properties name them.
		RegisterGeneratedDataReflection();
		RegisterGeneratedScriptReflection();
		GameSession::Register<HearthlandsSession>();
	}
}
