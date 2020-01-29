#include "Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>

#include <json.hpp>

#include <iomanip>
#include <iostream>
#include <fstream>

Application::Application(int width, int height)
	:window_(width, height), api_handler_(user_)
{
	if (!gladLoadGLLoader(GLADloadproc(glfwGetProcAddress))) {
		std::cout << "could not load GLAD";
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui_ImplGlfw_InitForOpenGL(window_.glfw_window_, true);
	ImGui_ImplOpenGL3_Init("#version 410");

	SetImGuiStyle();
	
	glfwSetWindowUserPointer(window_.glfw_window_, this);
	Load();
}

Application::~Application()
{
	Save();
	glfwTerminate();
}

void Application::Run()
{
	while (!glfwWindowShouldClose(window_.glfw_window_) && running_)
	{
		if (state_stack_.top() == State::GetLeagueData) {
			current_leagues_ = api_handler_.GetCurrentLeagues();

			if (current_leagues_.empty()) {
				state_stack_.push(State::Error);
			}
			else {
				//clear loaded selected league if does not exist. i.e metamorph league ends.
				if (!user_.selected_league_.empty()) {
					if (std::find(current_leagues_.begin(), current_leagues_.end(), user_.selected_league_) == current_leagues_.end()) {
						user_.selected_league_.clear();
					}
				}
				state_stack_.pop();
			}
		}

		if (Window::IsFocused()) {
			Render();
		}

		glfwPollEvents();

		//Must change font outside of ImGui Rendering
		//if (font_size_changed_) {
		//	font_size_changed_ = false;
		//
		//	ImGuiIO& io = ImGui::GetIO();
		//	delete io.Fonts;
		//	io.Fonts = new ImFontAtlas();
		//	io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Medium.ttf", font_size_);
		//	ImGui_ImplOpenGL3_CreateFontsTexture();
		//}
	}

}

void Application::Render()
{
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowSize({ (float)window_.width_, (float)window_.height_ });
	ImGui::Begin("Tab Appraiser", &running_, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
	ImGui::Separator();

	if (loading_price_data_) {
		LoadPriceData();
	}
	else {
		RenderStates();
	}
	
	ImVec2 pos = ImGui::GetWindowPos();
	if (pos.x != 0.0f || pos.y != 0.0f) {
		ImGui::SetWindowPos({ 0.0f, 0.0f });
		window_.Move(pos.x, pos.y);
	}

	if (window_.height_ != ImGui::GetCursorPosY()) {
		window_.ResizeHeight(ImGui::GetCursorPosY());
	}
	ImGui::End();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwSwapBuffers(window_.glfw_window_);
}

void Application::RenderStates()
{
	if (state_stack_.top() == State::Error) {
		ImGui::Text(api_handler_.error_msg_.c_str());
		if (ImGui::Button("Retry")) {
			state_stack_.pop();
		}
		if (ImGui::Button("Back")) {
			while (state_stack_.top() != State::Render) {
				state_stack_.pop();
			}
		}
	}
	else {
		ImGui::Text("Account: "); ImGui::SameLine(); ImGui::Text(user_.account_name_.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Change Account")) {
			changing_account_ = true;
		}

		if (changing_account_) {
			ImGui::Text("Please Enter Your POESESSID");
			static char sess_id_input[100];
			ImGui::InputText("##Input", sess_id_input, 100);

			if (ImGui::Button("OK")) {
				SetPOESESSID(sess_id_input);
				if (user_.account_name_.empty()) {
					state_stack_.push(State::Error);
				}
				else {
					changing_account_ = false;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				changing_account_ = false;
			}
		}

		if (!user_.account_name_.empty()) {
			ImGui::Text("League: ");
			ImGui::SameLine();

			if (ImGui::BeginCombo("##LeagueCombo", user_.selected_league_.empty() ? "Select a League" : user_.selected_league_.c_str())) {
				for (auto& league : current_leagues_) {
					if (ImGui::Selectable(league.c_str())) {
						user_.selected_league_ = league;

						user_.stash_tab_list_ = api_handler_.GetStashTabList();

						selected_stash_index_ = -1;
						stash_item_prices_.clear();

						if (user_.stash_tab_list_.empty()) {
							state_stack_.push(State::Error);
							user_.selected_league_.clear();
						}
						else {
							loading_price_data_ = true;
						}


					}
				}
				//selectable popup does not close if user clicks out of window and loses focus
				//must do manually
				if (!Window::IsFocused()) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndCombo();
			}
		}


		if (!user_.selected_league_.empty()) {
			ImGui::Text("Stash Tab: ");
			ImGui::SameLine();
			if (ImGui::BeginCombo("##StashTabsCombo", selected_stash_index_ == -1 ? "Select a Stash Tab" : user_.stash_tab_list_[selected_stash_index_].c_str())) {
				for (int i = 0; i < user_.stash_tab_list_.size(); i++) {
					if (ImGui::Selectable(user_.stash_tab_list_[i].c_str())) {
						selected_stash_index_ = i;
						stash_items_ = api_handler_.GetStashItems(selected_stash_index_);
						if (stash_items_.empty()) {
							state_stack_.push(State::Error);
						}
						stash_item_prices_ = GetItemPrices();
					}
				}
				//selectable popup does not close if user clicks out of window and loses focus
				//must do manually
				if (!Window::IsFocused()) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndCombo();
			}
		}

		ImGui::Separator();

		if (!stash_item_prices_.empty()) {
			ImGui::BeginChildFrame(1, { (float)window_.width_, 250 });
			for (const auto& item : stash_item_prices_) {
				ImGui::Text(item.first.c_str());
				ImGui::SameLine(200);
				ImGui::Text("%.1f", item.second);
			}
			ImGui::EndChildFrame();
		}
		else if (!ninja_data_.empty() && selected_stash_index_ != -1){
			ImGui::Text("No price data available.");
		}

		ImGui::Separator();

		ImGui::Text("Price Threshold: ");
		ImGui::SameLine();
		if (ImGui::InputInt("##PriceThreshold", &price_threshold_)) {
			stash_item_prices_ = GetItemPrices();
		}

		if (!user_.selected_league_.empty() && !ninja_data_.empty()) {
			if (ImGui::Button("Update Price Info")) {
				loading_price_data_ = true;
				stash_item_prices_ = GetItemPrices();
			}
		}
	}
}

void Application::LoadPriceData()
{
	static int iteration = 0;
	static std::array<const char*, 19> item_types = { "Currency", "Fragment",
											"Watchstone", "Oil", "Incubator",
											"Scarab", "Fossil", "Resonator",
											"Essence", "DivinationCard", "Prophecy",
											"SkillGem", "UniqueMap", "Map",
											"UniqueJewel", "UniqueFlask", "UniqueWeapon",
											"UniqueArmour", "UniqueAccessory" };

	ImGui::Text("Loading Price Data");
	ImGui::Text("%i / %i", iteration, item_types.size());

	std::unordered_map<std::string, float> temp_data;
	if (iteration < 2) {
		temp_data = api_handler_.GetCurrencyPriceData(item_types[iteration]);
	}
	else if (iteration < item_types.size()){
		temp_data = api_handler_.GetItemPriceData(item_types[iteration]);
	}
	else {
		iteration = 0;
		loading_price_data_ = false;
		return;
	}

	ninja_data_.insert(temp_data.begin(), temp_data.end());

	++iteration;
}

void Application::Save()
{
	std::ofstream file("save-data.json");

	nlohmann::json json;
	json["POESESSID"] = user_.POESESSID_;
	json["selectedLeague"] = user_.selected_league_;
	json["windowX"] = window_.x_pos_;
	json["windowY"] = window_.y_pos_;
	
	file << std::setw(4) << json << std::endl;
}

void Application::Load()
{
	state_stack_.push(State::Render);
	std::ifstream file("save-data.json");
	if (file.is_open()) {
		auto json = nlohmann::json::parse(file);

		if (json.count("POESESSID")) {
			user_.POESESSID_ = json["POESESSID"];
			if (!user_.POESESSID_.empty()) {
				SetPOESESSID(user_.POESESSID_.c_str());
			}
		}

		if (json.count("selectedLeague")) {
			user_.selected_league_ = json["selectedLeague"];
			if (!user_.selected_league_.empty()) {
				user_.stash_tab_list_ = api_handler_.GetStashTabList();
				loading_price_data_ = true;
				
			}
		}

		if (json.count("windowX") && json.count("windowY")) {
			window_.Move(json["windowX"], json["windowY"]);
		}
	}
	state_stack_.push(State::GetLeagueData);
}

void Application::SetImGuiStyle()
{
	ImGuiStyle* style = &ImGui::GetStyle();

	style->WindowPadding = ImVec2(2, 2);

	style->WindowRounding = 0.0f;
	style->Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.2f);
	style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
	style->Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
}

void Application::SetPOESESSID(const char* id)
{
	user_.POESESSID_ = id;
	user_.account_name_ = api_handler_.GetAccountName();
}

std::vector<std::pair<std::string, float>> Application::GetItemPrices()
{
	std::vector<std::pair<std::string, float>> item_price;
	
	for (const auto& item : stash_items_) {
		if (ninja_data_.count(item)) {
			if (ninja_data_[item] > price_threshold_) {
				item_price.push_back({ item, ninja_data_[item] });
			}
		}
	}

	std::sort(item_price.begin(), item_price.end(), [](auto& left, auto& right) {
		return left.second > right.second;
	});

	return item_price;
}




