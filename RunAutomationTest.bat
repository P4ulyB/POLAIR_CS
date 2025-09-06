@echo off
echo Running PACS LaunchArg Test...
"C:\Devops\UESource\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Devops\Projects\POLAIR_CS\POLAIR_CS.uproject" -ExecCmds="Automation RunTests PACS.LaunchArgs.Parse" -unattended -nopause -NullRHI -testexit="Automation Test Queue Empty" -log -log=AutomationTest.log -stdout -FullStdOutLogOutput
echo Test complete. Check Saved\Logs\AutomationTest.log for results.