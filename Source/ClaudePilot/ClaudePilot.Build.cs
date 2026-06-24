using UnrealBuildTool;

public class ClaudePilot : ModuleRules
{
	public ClaudePilot(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"ToolMenus",
			"MainFrame",
			"EditorWidgets",
			"PropertyEditor",
			"LevelEditor",
			"Projects",
			"HTTP",
			"Json",
		});
	}
}
