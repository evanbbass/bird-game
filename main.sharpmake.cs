using System.IO; // For Path.Combine
using Sharpmake; // Contains the entire Sharpmake object library.

[Generate]
public class BirdGameProject : Project
{
    public BirdGameProject()
    {
        // The name of the project in Visual Studio
        Name = "BirdGame";

        // The directory that contains the source code we want to build
        SourceRootPath = Path.Combine("[project.SharpmakeCsPath]", "src");

        // TODO trying to add the shaders to the project but this doesn't work??
        //AdditionalSourceRootPaths.Add(Path.Combine("[project.SharpmakeCsPath]", "assets"));

        // Specify the targets for which we want to generate a configuration for
        // Include both VS2017 and VS2022 since IG PCs are on 2017
        AddTargets(new Target(
            Platform.win64,
            DevEnv.vs2017 | DevEnv.vs2022,
            Optimization.Debug | Optimization.Release));
    }

	// Sets the properties of each configuration (conf) according to the target
	// This method is called once for every target specified by AddTargets
	[Configure]
	public void ConfigureAll(Project.Configuration conf, Target target)
	{
		conf.ProjectName = "BirdGame";

        // Specify where the generated project will be. Here we generate the
        // vcxproj in a /generated directory.
        if (target.DevEnv == DevEnv.vs2017)
        {
            conf.ProjectPath = Path.Combine("[project.SharpmakeCsPath]", "generated", "vs2017");
        }
        else if (target.DevEnv == DevEnv.vs2022)
        {
            conf.ProjectPath = Path.Combine("[project.SharpmakeCsPath]", "generated", "vs2022");
        }
        else
        {
            conf.ProjectPath = Path.Combine("[project.SharpmakeCsPath]", "generated", "other");
        }

		// Add include path
		conf.IncludePaths.Add(Path.Combine("[project.SharpmakeCsPath]", "include"));

		conf.Options.Add(Options.Vc.General.CharacterSet.Unicode);
		conf.Options.Add(Options.Vc.General.WarningLevel.Level3);
		conf.Options.Add(Options.Vc.General.TreatWarningsAsErrors.Enable);
        if (target.DevEnv == DevEnv.vs2017)
        {
            // For some reason for my VS2017, latest targets Windows 8.1, so specify a Windows 10 version
            conf.Options.Add(Options.Vc.General.WindowsTargetPlatformVersion.v10_0_17763_0);
        }
        else
		{
			conf.Options.Add(Options.Vc.General.WindowsTargetPlatformVersion.Latest);
		}

		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP17);
		conf.Options.Add(Options.Vc.Compiler.Exceptions.Enable);

		conf.Options.Add(Options.Vc.Linker.SubSystem.Windows);
		conf.Options.Add(Options.Vc.Linker.LargeAddress.SupportLargerThan2Gb);

		conf.PrecompHeader = "pch.h";
		conf.PrecompSource = "pch.cpp";

		conf.LibraryFiles.Add("d3d12");
		conf.LibraryFiles.Add("dxgi");
		conf.LibraryFiles.Add("d3dcompiler");
		conf.LibraryFiles.Add("dxguid");

        // Copy Assets folder to destination. Mirror the file structure
        conf.EventPostBuildExe.Add(
            new Configuration.BuildStepCopy(
                Path.Join("[project.SharpmakeCsPath]", "assets"),   // sourcePath
				Path.Join("[conf.TargetPath]", "assets"),           // destinationPath
                false,                                              // isNameSpecific (default)
                "*",                                                // copyPattern (default)
				true,                                               // fileCopy (default)
				true                                                // mirror
			 )
        );

        // Set the working directory in the user file so the program knows where to find the shader file
        conf.VcxprojUserFile = new Configuration.VcxprojUserFileSettings()
        {
            LocalDebuggerWorkingDirectory = "[conf.TargetPath]"
        };
    }
}

[Generate]
public class BirdGameSolution : Solution
{
	public BirdGameSolution()
	{
		// The name of the solution
		Name = "BirdGame";

		// As with the project, define which target this solution builds for.
		AddTargets(new Target(
			Platform.win64,
			DevEnv.vs2017 | DevEnv.vs2022,
			Optimization.Debug | Optimization.Release));
	}

	[Configure]
	public void ConfigureAll(Solution.Configuration conf, Target target)
	{
		// Puts the generated solution in the /generated folder too.
        if (target.DevEnv == DevEnv.vs2017)
        {
            conf.SolutionPath = Path.Combine("[solution.SharpmakeCsPath]", "generated", "vs2017");
        }
        else if (target.DevEnv == DevEnv.vs2022)
        {
            conf.SolutionPath = Path.Combine("[solution.SharpmakeCsPath]", "generated", "vs2022");
        }
        else
        {
            conf.SolutionPath = Path.Combine("[solution.SharpmakeCsPath]", "generated", "other");
        }

        // Adds the project described by BasicsProject into the solution
        conf.AddProject<BirdGameProject>(target);
	}
}

public static class Main
{
	[Sharpmake.Main]
	public static void SharpmakeMain(Sharpmake.Arguments arguments)
	{
		arguments.Generate<BirdGameSolution>();
	}
}
