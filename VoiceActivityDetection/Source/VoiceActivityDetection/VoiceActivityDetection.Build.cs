// Some copyright should be here...

using UnrealBuildTool;
using System.IO;
public class VoiceActivityDetection : ModuleRules
{
	private string ModulePath
	{
		get { return Path.GetDirectoryName(RulesCompiler.GetModuleFilename(this.GetType().Name)); }
	}

	private string ThirdPartyPath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../../Thirdparty")); }
	}

	public VoiceActivityDetection(TargetInfo Target)
	{

		PublicIncludePaths.AddRange(
			new string[] {
				"VoiceActivityDetection/Public"
				
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				"VoiceActivityDetection/Private",
				
				// ... add other private include paths required here ...
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject", "Engine", "Slate", "SlateCore", "Media", "MediaAssets", "Voice"
				// ... add private dependencies that you statically link with here ...	
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
				// ... add any modules that your module loads dynamically here ...
			}
			);
		PublicIncludePaths.Add(ThirdPartyPath);
		PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "speex", "include"));
		LoadLibs(Target);
	}

	public bool LoadLibs(TargetInfo Target)
	{
		bool isLibrarySupported = false;

		if ((Target.Platform == UnrealTargetPlatform.Win64))
		{
			isLibrarySupported = true;
			//Thirdparty/webrtc/x64/Release/lib/VAD.lib
			string PlatformString = (Target.Platform == UnrealTargetPlatform.Win64) ? "x64" : "x86";
			string LibrariesPath = Path.Combine(ThirdPartyPath, "webrtc", PlatformString, "Release");
			PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "VAD.lib"));
			LibrariesPath = Path.Combine(ThirdPartyPath, "speex");
			//speex/win32/VS2008/x64/
			PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "win32", "VS2008", PlatformString, "Release", "libspeex.lib"));
		}

		Definitions.Add(string.Format("WITH_VOICEACTIVITYDETECTION_BINDING={0}", isLibrarySupported ? 1 : 0));

		return isLibrarySupported;
	}

}
