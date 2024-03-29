/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

// Headers
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "scene_title.h"
#include "audio.h"
#include "bitmap.h"
#include "cache.h"
#include "filefinder.h"
#include "game_map.h"
#include "game_enemyparty.h"
#include "game_player.h"
#include "game_screen.h"
#include "game_switches.h"
#include "game_system.h"
#include "game_variables.h"
#include "graphics.h"
#include "input.h"
#include "ldb_reader.h"
#include "lmt_reader.h"
#include "main_data.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "reader_lcf.h"
#include "scene_battle.h"
#include "scene_load.h"
#include "scene_map.h"
#include "util_macro.h"
#include "window_command.h"

Scene_Title::Scene_Title() {
	type = Scene::Title;
}

void Scene_Title::Start() {
	LoadDatabase();

	static bool init = false;
	if (!init) {
		if (Data::system.ldb_id == 2003) {
			Output::Debug("Switching to Rpg2003 Interpreter");
			Player::engine = Player::EngineRpg2k3;
		}

		FileFinder::InitRtpPaths();
	}
	init = true;

	Main_Data::game_data.Setup();

	// Create Game System
	Game_System::Init();

	if (!Player::battle_test_flag) {
		CreateTitleGraphic();
		PlayTitleMusic();
	}

	CreateCommandWindow();
}

void Scene_Title::Continue() {
	// Clear the cache when the game returns to title screen
	// e.g. by pressing F12
	Cache::Clear();

	Start();
}

void Scene_Title::TransitionIn() {
	if (!Player::battle_test_flag) {
		Graphics::Transition(Graphics::TransitionErase, 1, true);
		Graphics::Transition(Graphics::TransitionFadeIn, 32);
	}
}

void Scene_Title::TransitionOut() {
	if (!Player::battle_test_flag) {
		Graphics::Transition(Graphics::TransitionFadeOut, 12, true);
	}
}

void Scene_Title::Resume() {
	command_window->SetVisible(true);
}

void Scene_Title::Suspend() {
	command_window->SetVisible(false);
}

void Scene_Title::Update() {
	if (Player::battle_test_flag) {
		PrepareBattleTest();
		return;
	}

	command_window->Update();

	if (Input::IsTriggered(Input::DECISION)) {
		switch (command_window->GetIndex()) {
		case 0: // New Game
			CommandNewGame();
			break;
		case 1: // Load Game
			CommandContinue();
			break;
		case 2: // Exit Game
			CommandShutdown();
		}
	}
}

void Scene_Title::LoadDatabase() {
	// Load Database
	Data::Clear();

	if (!FileFinder::IsRPG2kProject(FileFinder::GetProjectTree()) &&
		!FileFinder::IsEasyRpgProject(FileFinder::GetProjectTree())) {
		Output::Debug("%s is not a supported project", Main_Data::project_path.c_str());
 	}

	// Try loading EasyRPG project files first, then fallback to normal RPG Maker
	std::string edb = FileFinder::FindDefault(DATABASE_NAME_EASYRPG);
	std::string emt = FileFinder::FindDefault(TREEMAP_NAME_EASYRPG);

	bool easyrpg_project = !edb.empty() && !emt.empty();

	if (easyrpg_project) {
		if (!LDB_Reader::LoadXml(edb)) {
			Output::ErrorStr(LcfReader::GetError());
		}
		if (!LMT_Reader::LoadXml(emt)) {
			Output::ErrorStr(LcfReader::GetError());
		}
	}
	else {
		std::string ldb = FileFinder::FindDefault(DATABASE_NAME);
		std::string lmt = FileFinder::FindDefault(TREEMAP_NAME);

		if (!LDB_Reader::Load(ldb,
			ReaderUtil::GetEncoding(FileFinder::FindDefault(INI_NAME)))) {
			Output::ErrorStr(LcfReader::GetError());
		}
		if (!LMT_Reader::Load(lmt,
 			ReaderUtil::GetEncoding(FileFinder::FindDefault(INI_NAME)))) {
			Output::ErrorStr(LcfReader::GetError());
		}

	}
}

bool Scene_Title::CheckContinue() {
	EASYRPG_SHARED_PTR<FileFinder::ProjectTree> tree;
	tree = FileFinder::CreateProjectTree(Main_Data::project_path, false);

	for (int i = 1; i <= 15; i++)
	{
		std::stringstream ss;
		ss << "Save" << (i <= 9 ? "0" : "") << i << ".lsd";

		if (!FileFinder::FindDefault(*tree, ss.str()).empty()) {
			return true;
		}
	}
	return false;
}

void Scene_Title::CreateTitleGraphic() {
	// Load Title Graphic
	if (!title) // No need to recreate Title on Resume
	{
		title.reset(new Sprite());
		title->SetBitmap(Cache::Title(Data::system.title_name));
	}
}

void Scene_Title::CreateCommandWindow() {
	// Create Options Window
	std::vector<std::string> options;
	options.push_back(Data::terms.new_game);
	options.push_back(Data::terms.load_game);
	options.push_back(Data::terms.exit_game);

	command_window.reset(new Window_Command(options));
	command_window->SetX(160 - command_window->GetWidth() / 2);
	command_window->SetY(224 - command_window->GetHeight());

	// Enable load game if available
	continue_enabled = CheckContinue();
	if (continue_enabled) {
		command_window->SetIndex(1);
	} else {
		command_window->DisableItem(1);
	}

	// Set the number of frames for the opening animation to last
	command_window->SetOpenAnimation(32);

	command_window->SetVisible(false);
}

void Scene_Title::PlayTitleMusic() {
	// Play music
	Game_System::BgmPlay(Data::system.title_music);
}

bool Scene_Title::CheckValidPlayerLocation() {
	return (Data::treemap.start.party_map_id > 0);
}

void Scene_Title::PrepareBattleTest() {
	Player::CreateGameObjects();
	//Game_Troop::can_escape = true;

	Scene::Push(Scene_Battle::Create(), true);
}

void Scene_Title::CommandNewGame() {
	if (!CheckValidPlayerLocation()) {
		Output::Warning("The game has no start location set.");
	} else {
		Game_System::SePlay(Main_Data::game_data.system.decision_se);
		Game_System::BgmStop();
		Graphics::SetFrameCount(0);
		Player::CreateGameObjects();
		Game_Map::Setup(Data::treemap.start.party_map_id);
		Main_Data::game_player->MoveTo(
			Data::treemap.start.party_x, Data::treemap.start.party_y);
		Main_Data::game_player->Refresh();
		Game_Map::PlayBgm();
		Scene::Push(EASYRPG_MAKE_SHARED<Scene_Map>());
	}
}

void Scene_Title::CommandContinue() {
	if (continue_enabled) {
		Game_System::SePlay(Main_Data::game_data.system.decision_se);
	} else {
		Game_System::SePlay(Main_Data::game_data.system.buzzer_se);
		return;
	}

	// Change scene
	Scene::Push(EASYRPG_MAKE_SHARED<Scene_Load>());
}

void Scene_Title::CommandShutdown() {
	Game_System::SePlay(Main_Data::game_data.system.decision_se);
	Audio().BGS_Fade(800);
	Scene::Pop();
}
