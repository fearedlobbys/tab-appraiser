#define NOMINMAX
#define RAPIDJSON_NOMEMBERITERATORCLASS

#include "ApiHandler.h"

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <fstream>
#include <iomanip>
#include <filesystem>
#include <array>

void ApiHandler::SetPOESESSIDCookie()
{
	http_.SetCookie("POESESSID=" + user_.POESESSID_);
}

std::string ApiHandler::GetAccountName()
{
	SetPOESESSIDCookie();

	auto data = http_.GetData("https://www.pathofexile.com/my-account");
	std::string temp = "/account/view-profile/";
	const int temp_length = 22;

	auto temp_pos = data.find(temp);
	if (temp_pos != std::string::npos) {
		auto substring = data.substr(temp_pos + temp_length, 100);

		std::string acc_name;
		for (auto& character : substring) {
			if (character == '\"') {
				return acc_name;
			}
			acc_name += character;
		}
	}
	return "Unable to find account name";
}

std::vector<std::string> ApiHandler::GetCurrentLeagues()
{
	auto data = http_.GetData("http://api.pathofexile.com/leagues");
	rapidjson::Document json;
	json.ParseInsitu(data.data());

	std::vector<std::string> leagues;
	for (const auto& league : json.GetArray()) {
		std::string league_name = league["id"].GetString();
		if (league_name.find("SSF") == std::string::npos) {
			leagues.push_back(league_name);
		}
	}

	return leagues;
}

std::vector<std::string> ApiHandler::GetStashTabList()
{
	auto data = http_.GetData("https://www.pathofexile.com/character-window/get-stash-items?accountName=" + user_.account_name_ + "&realm=pc&league=" + user_.selected_league_ + "&tabs=1&tabIndex=0");
	rapidjson::Document json;
	json.ParseInsitu(data.data());

	std::vector<std::string> tab_list;
	for (const auto& tab : json["tabs"].GetArray()) {
		tab_list.push_back(tab["n"].GetString());
	}

	return tab_list;

}

std::vector<std::string> ApiHandler::GetStashItems(int index)
{
	auto data = http_.GetData("https://www.pathofexile.com/character-window/get-stash-items?accountName=" + user_.account_name_ + "&realm=pc&league=" + user_.selected_league_ + "&tabs=0&tabIndex=" + std::to_string(index));
	rapidjson::Document json;
	json.ParseInsitu(data.data());

	std::vector<std::string> item_list;
	for (const auto& item : json["items"].GetArray()) {
		if (item["identified"] == false) {
			continue;
		}
		if (item["name"] != "") {
			item_list.push_back(item["name"].GetString());
		}
		else {
			item_list.push_back(item["typeLine"].GetString());
		}
	}

	return item_list;
}

std::unordered_map<std::string, float> ApiHandler::GetPriceData(const std::string& league)
{
	std::string base_url = "https://poe.ninja/api/data/";
	std::array<std::string, 2> currencies = { "Currency", "Fragment" };
	std::array<std::string, 20> items = { "Watchstone", "Oil", "Incubator", 
											"Scarab", "Fossil", "Resonator", 
											"Essence", "DivinationCard", "Prophecy", 
											"SkillGem", "BaseType", "HelmetEnchant", 
											"UniqueMap", "Map", "UniqueJewel", 
											"UniqueFlask", "UniqueWeapon", "UniqueArmour", 
											"UniqueAccessory", "Beast" };

	std::unordered_map<std::string, float> price_info;

	if (!std::filesystem::exists("PriceData/" + league)) {
		std::filesystem::create_directories("PriceData/" + league);
	}
	auto league_encoded = league;
	if (league_encoded.find(" ") != std::string::npos) {
		league_encoded.replace(league.find(" "), 1, "%20");
	}

	for (const auto& currency : currencies) {

		auto data = http_.GetData(base_url + "currencyoverview?league=" + league_encoded + "&type=" + currency);
		rapidjson::Document json;
		json.ParseInsitu(data.data());

		rapidjson::Document price_data;
		price_data.SetObject();
		for (const auto& line : json["lines"].GetArray()){
			rapidjson::Value temp;
			rapidjson::Value item(line["currencyTypeName"].GetString(), price_data.GetAllocator());
			rapidjson::Value price(line["chaosEquivalent"].GetFloat());
			price_data.AddMember(item.Move(), price.Move(), price_data.GetAllocator());
		}

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> jsonWriter(buffer);
		price_data.Accept(jsonWriter);

		std::ofstream file("PriceData/" + league + std::string("/") + currency + ".json");
		file << buffer.GetString() << std::endl;
	}

	for (const auto& item : items) {
		auto data = http_.GetData(base_url + "itemoverview?league=" + league_encoded + "&type=" + item);
		rapidjson::Document json;
		json.ParseInsitu(data.data());

		rapidjson::Document price_data;
		price_data.SetObject();
		for (const auto& line : json["lines"].GetArray()) {
			rapidjson::Value temp;
			rapidjson::Value item(line["name"].GetString(), price_data.GetAllocator());
			rapidjson::Value price(line["chaosValue"].GetFloat());
			price_data.AddMember(item.Move(), price.Move(), price_data.GetAllocator());
		}

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> jsonWriter(buffer);
		price_data.Accept(jsonWriter);

		std::ofstream file("PriceData/" + league + std::string("/") + item + ".json");
		file << buffer.GetString() << std::endl;
	
	}
	return price_info;
}
