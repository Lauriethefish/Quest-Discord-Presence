# Builds a .qmod file for loading with QuestPatcher or BMBF
$NDKPath = Get-Content $PSScriptRoot/ndkpath.txt

$buildScript = "$NDKPath/build/ndk-build"
if (-not ($PSVersionTable.PSEdition -eq "Core")) {
    $buildScript += ".cmd"
}

$ArchiveName = "discord-presence_v0.3.2.qmod"
$TempArchiveName = "discord-presence_v0.3.2.qmod.zip"

& $buildScript NDK_PROJECT_PATH=$PSScriptRoot APP_BUILD_SCRIPT=$PSScriptRoot/Android.mk NDK_APPLICATION_MK=$PSScriptRoot/Application.mk
Compress-Archive -Path "./libs/arm64-v8a/libdiscord-presence.so", "./libs/arm64-v8a/libbeatsaber-hook_1_3_5.so", "./mod.json", "./module.json", "./cover.png" -DestinationPath $TempArchiveName -Force
Move-Item $TempArchiveName $ArchiveName -Force