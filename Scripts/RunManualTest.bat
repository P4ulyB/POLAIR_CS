@echo off
echo Running PACS Manual Launch Args Test...
"C:\Devops\UESource\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Devops\Projects\POLAIR_CS\POLAIR_CS.uproject" -ExecCmds="PACS.TestLaunchArgs" -unattended -nopause -NullRHI -testexit="Automation Test Queue Empty" -log -log=ManualTest.log -stdout
echo Test complete. Check Saved\Logs\ManualTest.log for results.
timeout /t 5