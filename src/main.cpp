#include "beatsaber-hook/shared/utils/typedefs.h"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "beatsaber-hook/shared/utils/utils.h"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/typedefs.h"
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "UnityEngine/Resources.hpp"

#include "System/Action_1.hpp"

#include "GlobalNamespace/IConnectedPlayer.hpp"
#include "GlobalNamespace/MultiplayerPlayersManager.hpp"
#include "GlobalNamespace/MultiplayerSessionManager.hpp"
#include "GlobalNamespace/GameServerLobbyFlowCoordinator.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/StandardLevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/IBeatmapLevel.hpp"
#include "GlobalNamespace/MultiplayerLocalActivePlayerGameplayManager.hpp"
#include "GlobalNamespace/StandardLevelGameplayManager.hpp"
#include "GlobalNamespace/TutorialSongController.hpp"
#include "GlobalNamespace/MissionLevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/MissionLevelGameplayManager.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/ILobbyPlayersDataModel.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/LobbyPlayersDataModel.hpp"
#include "System/Collections/Generic/IReadOnlyDictionary_2.hpp"
using namespace GlobalNamespace;

#include "modloader/shared/modloader.hpp"

#include <string>
#include <optional>
#include "presencemanager.hpp"

static ModInfo modInfo;
static Configuration& getConfig() {
    static Configuration config(modInfo);
    return config;
}

static Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}
static PresenceManager* presenceManager = nullptr;
static LevelInfo selectedLevel;

// Converts the int representing an IBeatmapDifficulty into a string
std::string difficultyToString(BeatmapDifficulty difficulty)  {
    switch(difficulty)  {
        case BeatmapDifficulty::Easy:
            return "Easy";
        case BeatmapDifficulty::Normal:
            return "Normal";
        case BeatmapDifficulty::Hard:
            return "Hard";
        case BeatmapDifficulty::Expert:
            return "Expert";
        case BeatmapDifficulty::ExpertPlus:
            return "Expert+";
    }
    return "Unknown";
}

// Define the current level by finding info from the IBeatmapLevel object
MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {
    StandardLevelDetailView_RefreshContent(self);
    IPreviewBeatmapLevel* level = reinterpret_cast<IPreviewBeatmapLevel*>(self->level);
    if(!level) {
        return;
    }

    // Check if the level is an instance of BeatmapLevelSO
    selectedLevel.name = to_utf8(csstrtostr(level->get_songName()));
    selectedLevel.levelAuthor = to_utf8(csstrtostr(level->get_levelAuthorName()));
    selectedLevel.songAuthor = to_utf8(csstrtostr(level->get_songAuthorName()));
}

static int currentFrame = -1;

// Called when starting a non-multiplayer level
MAKE_HOOK_MATCH(MenuTransitionsHelper_StartStandardLevel, static_cast<void (GlobalNamespace::MenuTransitionsHelper::*)(::StringW, ::GlobalNamespace::IDifficultyBeatmap*, ::GlobalNamespace::IPreviewBeatmapLevel*, ::GlobalNamespace::OverrideEnvironmentSettings*, ::GlobalNamespace::ColorScheme*, ::GlobalNamespace::GameplayModifiers*, ::GlobalNamespace::PlayerSpecificSettings*, ::GlobalNamespace::PracticeSettings*, ::StringW, bool, bool, ::System::Action*, ::System::Action_2<::GlobalNamespace::StandardLevelScenesTransitionSetupDataSO*, ::GlobalNamespace::LevelCompletionResults*>*)>(&GlobalNamespace::MenuTransitionsHelper::StartStandardLevel),
        void,
        MenuTransitionsHelper* self,
        StringW gameMode,
        IDifficultyBeatmap* difficultyBeatmap,
        IPreviewBeatmapLevel* previewBeatmapLevel,
        OverrideEnvironmentSettings* overrideEnvironmentSettings,
        ColorScheme* overrideColorScheme,
        GameplayModifiers* gameplayModifiers,
        PlayerSpecificSettings* playerSpecificSettings,
        PracticeSettings* practiceSettings,
        StringW backButtonText,
        bool useTestNoteCutSoundEffects,
        bool startPaused,
        System::Action* beforeSceneSwitchCallback,
        System::Action_2<StandardLevelScenesTransitionSetupDataSO*, LevelCompletionResults*>* levelFinishedCallback) {
            getLogger().info("Song Started");
    currentFrame = -1;
    BeatmapDifficulty difficulty = difficultyBeatmap->get_difficulty();
    selectedLevel.selectedDifficulty = difficultyToString(difficulty);

    // Set the currently playing level to the selected one, since we are in a song
    presenceManager->statusLock.lock();
    presenceManager->playingLevel.emplace(selectedLevel);
    presenceManager->isPractice = practiceSettings; // If practice settings isn't null, then we're in practice mode

    if(presenceManager->isPractice) {
        getLogger().info("Practice mode is enabled!");
    }
    presenceManager->statusLock.unlock();

    MenuTransitionsHelper_StartStandardLevel(
        self,
        gameMode,
        difficultyBeatmap,
        previewBeatmapLevel,
        overrideEnvironmentSettings,
        overrideColorScheme,
        gameplayModifiers,
        playerSpecificSettings,
        practiceSettings,
        backButtonText,
        useTestNoteCutSoundEffects,
        startPaused,
        beforeSceneSwitchCallback,
        levelFinishedCallback
    );
}

// Called when starting a multiplayer level
MAKE_HOOK_MATCH(MenuTransitionsHelper_StartMultiplayerLevel,  static_cast<void (GlobalNamespace::MenuTransitionsHelper::*)(::StringW, ::GlobalNamespace::IPreviewBeatmapLevel*, ::GlobalNamespace::BeatmapDifficulty, ::GlobalNamespace::BeatmapCharacteristicSO*, ::GlobalNamespace::IDifficultyBeatmap*, ::GlobalNamespace::ColorScheme*, ::GlobalNamespace::GameplayModifiers*, ::GlobalNamespace::PlayerSpecificSettings*, ::GlobalNamespace::PracticeSettings*, ::StringW, bool, ::System::Action*, ::System::Action_2<::GlobalNamespace::MultiplayerLevelScenesTransitionSetupDataSO*, ::GlobalNamespace::MultiplayerResultsData*>*, ::System::Action_1<::GlobalNamespace::DisconnectedReason>*)>(&GlobalNamespace::MenuTransitionsHelper::StartMultiplayerLevel), void,
    MenuTransitionsHelper* self,
    StringW gameMode,
    IPreviewBeatmapLevel* previewBeatmapLevel,
    BeatmapDifficulty beatmapDifficulty,
    BeatmapCharacteristicSO* beatmapCharacteristic,
    IDifficultyBeatmap* difficultyBeatmap,
    ColorScheme* overrideColorScheme,
    GameplayModifiers* gameplayModifiers,
    PlayerSpecificSettings* playerSpecificSettings,
    PracticeSettings* practiceSettings,
    StringW backButtonText,
    bool useTestNoteCutSoundEffects,
    System::Action* beforeSceneSwitchCallback,
    System::Action_2<MultiplayerLevelScenesTransitionSetupDataSO*, MultiplayerResultsData*>* levelFinishedCallback,
    System::Action_1<DisconnectedReason>* didDisconnectCallback) {

    getLogger().info("Multiplayer Song Started");
    selectedLevel.selectedDifficulty = difficultyToString(beatmapDifficulty);
    presenceManager->statusLock.lock();
    presenceManager->playingLevel.emplace(selectedLevel);
    presenceManager->statusLock.unlock();
    
    MenuTransitionsHelper_StartMultiplayerLevel(
        self,
        gameMode,
        previewBeatmapLevel,
        beatmapDifficulty,
        beatmapCharacteristic,
        difficultyBeatmap,
        overrideColorScheme,
        gameplayModifiers,
        playerSpecificSettings,
        practiceSettings,
        backButtonText,
        useTestNoteCutSoundEffects,
        beforeSceneSwitchCallback,
        levelFinishedCallback,
        didDisconnectCallback
    );
}

void handleLobbyPlayersDataModelDidChange(IMultiplayerSessionManager* multiplayerSessionManager, StringW userId) {
    presenceManager->statusLock.lock();
    presenceManager->multiplayerLobby->numberOfPlayers = multiplayerSessionManager->get_connectedPlayerCount() + 1;
    presenceManager->statusLock.unlock();
}

// Reset the lobby back to null when we leave back to the menu
void onLobbyDisconnect() {
    getLogger().info("Left Multiplayer lobby");
    presenceManager->statusLock.lock();
    presenceManager->multiplayerLobby = std::nullopt;
    presenceManager->statusLock.unlock();
}

MAKE_HOOK_MATCH(GameServerLobbyFlowCoordinator_DidActivate, &GameServerLobbyFlowCoordinator::DidActivate, void, GameServerLobbyFlowCoordinator* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling)    {    
    getLogger().info("Joined multiplayer lobby");

    // TODO avoid FindObjectsOfTypeAll calls if possible
    // Not too much of an issue since we only do it once on multiplayer lobby start, but still not ideal

    
    // Used for updating current player count in the DidChange event
    LobbyPlayersDataModel* lobbyPlayersDataModel = reinterpret_cast<LobbyPlayersDataModel*>(self->lobbyPlayersDataModel);
    
    // Used for getting max player count
    // Previously used for getting current player count by listening to player connections/disconnections, however this isn't reliable, and yielded negative player counts
    IMultiplayerSessionManager* sessionManager = lobbyPlayersDataModel->multiplayerSessionManager;

    int maxPlayers = sessionManager->get_maxPlayerCount();
    int numActivePlayers = sessionManager->get_connectedPlayerCount();

    // Set the number of players in this lobby
    MultiplayerLobbyInfo lobbyInfo;
    lobbyInfo.numberOfPlayers = numActivePlayers + 1;
    lobbyInfo.maxPlayers = maxPlayers;
    presenceManager->statusLock.lock();
    presenceManager->multiplayerLobby.emplace(lobbyInfo);
    presenceManager->statusLock.unlock();
    
    // Used to update player count
    lobbyPlayersDataModel->add_didChangeEvent(il2cpp_utils::MakeDelegate<System::Action_1<StringW>*>(classof(System::Action_1<StringW>*), sessionManager, handleLobbyPlayersDataModelDidChange));

    // Register disconnect from lobby event
    sessionManager->add_disconnectedEvent(
        il2cpp_utils::MakeDelegate<System::Action_1<GlobalNamespace::DisconnectedReason>*>(classof(System::Action_1<GlobalNamespace::DisconnectedReason>*), static_cast<Il2CppObject*>(nullptr), onLobbyDisconnect)
    );

    GameServerLobbyFlowCoordinator_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);
}

// Called on standard level end
MAKE_HOOK_MATCH(StandardLevelGameplayManager_OnDestroy, &StandardLevelGameplayManager::OnDestroy, void, StandardLevelGameplayManager* self) {
    getLogger().info("Song Ended");
    presenceManager->statusLock.lock();
    presenceManager->playingLevel = std::nullopt; // Reset the currently playing song to None
    presenceManager->paused = false; // If we are pasued, unpause us, since we are returning to the menu
    presenceManager->statusLock.unlock();
    StandardLevelGameplayManager_OnDestroy(self);
}

// Called on multiplayer level end
MAKE_HOOK_MATCH(MultiplayerLocalActivePlayerGameplayManager_OnDisable, &MultiplayerLocalActivePlayerGameplayManager::OnDisable, void, MultiplayerLocalActivePlayerGameplayManager* self) {
    getLogger().info("Multiplayer Song Ended");
    presenceManager->statusLock.lock();
    presenceManager->playingLevel = std::nullopt; // Reset the currently playing song to None
    presenceManager->paused = false; // If we are pasued, unpause us, since we are returning to the menu
    presenceManager->statusLock.unlock();
    MultiplayerLocalActivePlayerGameplayManager_OnDisable(self);
}

// Called on tutorial start
MAKE_HOOK_MATCH(TutorialSongController_Awake, &TutorialSongController::Awake, void, TutorialSongController* self)   {
    getLogger().info("Tutorial starting");
    presenceManager->statusLock.lock();
    presenceManager->playingTutorial = true;
    presenceManager->statusLock.unlock();
    TutorialSongController_Awake(self);
}

MAKE_HOOK_MATCH(TutorialSongController_OnDestroy, &TutorialSongController::OnDestroy, void, TutorialSongController* self)   {
    getLogger().info("Tutorial ending");
    presenceManager->statusLock.lock();
    presenceManager->playingTutorial = false;
    presenceManager->paused = false; // If we are pasued, unpause us, since we are returning to the menu
    presenceManager->statusLock.unlock();
    TutorialSongController_OnDestroy(self);
}

MAKE_HOOK_MATCH(MissionLevelScenesTransitionSetupDataSO_Init, &MissionLevelScenesTransitionSetupDataSO::Init, void,
    MissionLevelScenesTransitionSetupDataSO* self,
    StringW missionId,
    GlobalNamespace::IDifficultyBeatmap* difficultyBeatmap,
    GlobalNamespace::IPreviewBeatmapLevel* previewBeatmapLevel,
    ArrayW<GlobalNamespace::MissionObjective*> missionObjectives,
    GlobalNamespace::ColorScheme* overrideColorScheme,
    GlobalNamespace::GameplayModifiers* gameplayModifiers,
    GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings,
    StringW backButtonText)   {
    getLogger().info("Campaign level starting");
    currentFrame = -1;
    presenceManager->statusLock.lock();
    presenceManager->playingCampaign = true;
    presenceManager->statusLock.unlock();

    MissionLevelScenesTransitionSetupDataSO_Init(self, missionId, difficultyBeatmap, previewBeatmapLevel, missionObjectives, overrideColorScheme, gameplayModifiers, playerSpecificSettings, backButtonText);
}

// Called upon mission levels (campaign levels) ending.
MAKE_HOOK_MATCH(MissionLevelGameplayManager_OnDestroy, &MissionLevelGameplayManager::OnDestroy, void, MissionLevelGameplayManager* self)   {
    getLogger().info("Campaign level ending");
    presenceManager->statusLock.lock();
    presenceManager->playingCampaign = false;
    presenceManager->paused = false; // If we are paused, unpause us, since we are returning to the menu
    presenceManager->statusLock.unlock();
    MissionLevelGameplayManager_OnDestroy(self);
}

MAKE_HOOK_MATCH(PauseController_Pause, &PauseController::Pause, void, PauseController* self)   {
    getLogger().info("Game paused");
    presenceManager->statusLock.lock();
    presenceManager->paused = true;
    presenceManager->statusLock.unlock();
    PauseController_Pause(self);
}

MAKE_HOOK_MATCH(PauseController_HandlePauseMenuManagerDidPressContinueButton, &PauseController::HandlePauseMenuManagerDidPressContinueButton, void, PauseController* self)   {
    getLogger().info("Game resumed");
    presenceManager->statusLock.lock();
    presenceManager->paused = false;
    presenceManager->statusLock.unlock();
    PauseController_HandlePauseMenuManagerDidPressContinueButton(self);
}

// Used to update song time - this is called every frame and is in a convenient place allowing us to easily get the song time
MAKE_HOOK_MATCH(AudioTimeSyncController_Update, &AudioTimeSyncController::Update, void, AudioTimeSyncController* self) {
    AudioTimeSyncController_Update(self);
    // Only update the time every 36 frames or so (0.5 seconds on Q1, shorter on Q2 but whatever) to avoid log spam
    constexpr int TIME_UPDATE_INTERVAL = 36;

    currentFrame++;
    if(currentFrame % TIME_UPDATE_INTERVAL != 0) {return;}
    currentFrame = 0;

    float time = self->get_songTime();
    float endTime = self->get_songEndTime();

    presenceManager->statusLock.lock();
    presenceManager->timeLeft = (int) (endTime - time);
    presenceManager->statusLock.unlock();
}

void saveDefaultConfig()  {
    getLogger().info("Creating config file . . .");
    ConfigDocument& config = getConfig().config;
    auto& alloc = config.GetAllocator();
    // If the config has already been created, don't overwrite it
    if(config.HasMember("multiplayerLevelPresence")) {
        getLogger().info("Config file already exists");
        return;
    }
    config.RemoveAllMembers();
    config.SetObject();
    // Create the sections of the config file for each type of presence
    rapidjson::Value levelPresence(rapidjson::kObjectType);
    levelPresence.AddMember("details", "Playing {mapName} ({mapDifficulty})", alloc);
    levelPresence.AddMember("state",  "By: {mapAuthor} {paused?}", alloc);
    config.AddMember("standardLevelPresence", levelPresence, alloc);

    rapidjson::Value practicePresence(rapidjson::kObjectType);
    practicePresence.AddMember("details", "Practising {mapName} ({mapDifficulty})", alloc);
    practicePresence.AddMember("state",  "By: {mapAuthor} {paused?}", alloc);
    config.AddMember("practicePresence", practicePresence, alloc);

    rapidjson::Value multiLevelPresence(rapidjson::kObjectType);
    multiLevelPresence.AddMember("details", "Playing multiplayer: ({numPlayers}/{maxPlayers})", alloc);
    multiLevelPresence.AddMember("state",  "{mapName} - {mapDifficulty} {paused?}", alloc);
    config.AddMember("multiplayerLevelPresence", multiLevelPresence, alloc);

    rapidjson::Value missionPresence(rapidjson::kObjectType);
    missionPresence.AddMember("details", "Playing Campaign", alloc);
    missionPresence.AddMember("state",  "{paused?}", alloc);
    config.AddMember("missionLevelPresence", missionPresence, alloc);

    rapidjson::Value tutorialPresence(rapidjson::kObjectType);
    tutorialPresence.AddMember("details", "Playing Tutorial", alloc);
    tutorialPresence.AddMember("state",  "{paused?}", alloc);
    config.AddMember("tutorialPresence", tutorialPresence, alloc);

    rapidjson::Value multiLobbyPresence(rapidjson::kObjectType);
    multiLobbyPresence.AddMember("details", "Multiplayer - In Lobby", alloc);
    multiLobbyPresence.AddMember("state",  "with ({numPlayers}/{maxPlayers}) players", alloc);
    config.AddMember("multiplayerLobbyPresence", multiLobbyPresence, alloc);

    rapidjson::Value menuPresence(rapidjson::kObjectType);
    menuPresence.AddMember("details", "In Menu", alloc);
    menuPresence.AddMember("state",  "", alloc);
    config.AddMember("menuPresence", menuPresence, alloc);

    getConfig().Write();
    getLogger().info("Config file created");
}

extern "C" void setup(ModInfo& info) {
    info.id = ID;
    info.version = VERSION;
    modInfo = info;
    getLogger().info("Modloader name: %s", Modloader::getInfo().name.c_str());
    getConfig().Load();
    saveDefaultConfig(); // Create the default config file

    getLogger().info("Completed setup!");
}

extern "C" void load() {
    getLogger().debug("Installing hooks...");
    il2cpp_functions::Init();

    // Install our function hooks
    Logger& logger = getLogger();
    INSTALL_HOOK(logger, StandardLevelDetailView_RefreshContent);
    INSTALL_HOOK(logger, MenuTransitionsHelper_StartStandardLevel);
    INSTALL_HOOK(logger, StandardLevelGameplayManager_OnDestroy);
    INSTALL_HOOK(logger, MissionLevelScenesTransitionSetupDataSO_Init);
    INSTALL_HOOK(logger, MissionLevelGameplayManager_OnDestroy);
    INSTALL_HOOK(logger, TutorialSongController_Awake);
    INSTALL_HOOK(logger, TutorialSongController_OnDestroy);
    INSTALL_HOOK(logger, PauseController_Pause);
    INSTALL_HOOK(logger, PauseController_HandlePauseMenuManagerDidPressContinueButton);
    INSTALL_HOOK(logger, AudioTimeSyncController_Update);
    INSTALL_HOOK(logger, MenuTransitionsHelper_StartMultiplayerLevel);
    INSTALL_HOOK(logger, GameServerLobbyFlowCoordinator_DidActivate);
    INSTALL_HOOK(logger, MultiplayerLocalActivePlayerGameplayManager_OnDisable);

    getLogger().debug("Installed all hooks!");
    presenceManager = new PresenceManager(getLogger(), getConfig().config);
}