#pragma once

#include "Window.h"
#include "ApiHandler.h"

#include <imgui.h>
#include <string>
#include <unordered_map>
#include <stack>

class Application
{
public:

	Application(int width, int height);
	~Application();

	void SetImGuiStyle();
	void SetPOESESSID(const char* id);
	std::vector<std::pair<std::string, float>> GetItemPrices();

	void Run();
	void Render();
	void RenderErrors();

	void Save();
	void Load();

private:
	bool running_ = true;
	bool changing_account_ = false;
	std::vector<std::string> current_leagues_;
	std::vector<std::string> stash_items_;
	std::vector<std::pair<std::string, float>> stash_item_prices_;
	std::unordered_map<std::string, float> ninja_data_;
	int selected_stash_index_= -1;
	int price_threshold_ = 0;
	std::stack<Error> errors_;

	UserData user_;
	Window window_;
	ApiHandler api_handler_;
};