class CfgPatches
{
	class SoZ_SmartAI_Scripts
	{
		units[]={};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]=
		{
			"DayZExpansion_AI_Scripts"
		};
	};
};
class CfgMods
{
	class SoZ_SmartAI
	{
		dir="SoZ_SmartAI";
		name="Sons of Zombieland - Smart AI";
		type="mod";
		dependencies[]=
		{
			"Game",
			"World",
			"Mission"
		};
		class defs
		{
			class gameScriptModule
			{
				files[]=
				{
					"SoZ_SmartAI/Scripts/3_Game"
				};
			};
			class worldScriptModule
			{
				files[]=
				{
					"SoZ_SmartAI/Scripts/4_World"
				};
			};
		};
	};
};
