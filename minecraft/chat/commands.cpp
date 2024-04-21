#include "commands.h"
#include "../../server/minecraft_server.h"

bool Commands::StopCommand::execute(const std::vector<std::string>& args) {
	MinecraftServer::get().stop();
	return true;
}

void Commands::Command::printData() {
	Logger logger = MinecraftServer::get().getLogger();
	logger.info(name + ": ");
	logger.info("	Description: " + desc);
	logger.info("	Usage: " + usage);
	std::string aliasString;
	for (const std::string& alias : aliases) aliasString += alias + ", ";
	aliasString.pop_back();
	aliasString.pop_back();
	logger.info("	Aliases: " + aliasString);
}

bool Commands::ReloadCommand::execute(const std::vector<std::string>& args) {
	Logger logger = MinecraftServer::get().getLogger();
	if (args.size() < 1) {
		MinecraftServer::get().getLogger().info(RED "Not enough arguments! Expected an option!");
		return true;
	}

	std::string arg1 = args[0];
	if (arg1 == "reload") {
		logger.info("Reloading config...");
		MinecraftServer::get().loadConfig();
		logger.info("Reloaded config!");
	} else if (arg1 == "set" || arg1 == "setvalue") {
		if (args.size() < 3) {
			logger.info(RED "Not enough arguments! Expected a value name and a value!");
			return true;
		}

		std::unordered_map<std::string, std::string> conf = MinecraftServer::get().getConfig().getMap();
		auto iter = conf.find(args[1]);
		if (iter != conf.end()) {
			conf[args[1]] = conf[args[2]];
			logger.info("Successfully set the config value '" + args[1] + "' to '" + args[2] + "'!");
			MinecraftServer::get().getConfig().save();
		} else {
			logger.info(RED "Value '" + args[1] + "' doesn't exist in the config!");
		}
	}
	return true;
}

bool Commands::HelpCommand::execute(const std::vector<std::string>& args) {
	int page = 0, maxPage = MinecraftServer::get().getCommandList().size() / 5 + 1;
	if (MinecraftServer::get().getCommandList().size() % 5 == 0) maxPage--;
	if (args.size() != 0) {
		try {
			page = std::stoi(args[0]);
			page--;
		} catch (const std::invalid_argument& exception) {
			MinecraftServer::get().getLogger().info(RED "'" + args[0] + "' is not a valid integer!");
			return true;
		}
		if (page >= maxPage) {
			MinecraftServer::get().getLogger().info(RED "'" + args[0] + "' is not a valid page number! There are a total of " + std::to_string(maxPage) + " pages.");
			return true;
		}
	}
	MinecraftServer::get().getLogger().info("Showing page " + std::to_string(page + 1) + " of " + std::to_string(maxPage));
	int end;
	if ((page * 5) + 5 > MinecraftServer::get().getCommandList().size() - 1) end = MinecraftServer::get().getCommandList().size() - 1;
	else end = (page * 5) + 5;
	for (int i = page * 5; i <= end; i++)
		MinecraftServer::get().getCommandList()[i]->printData();
	return true;
}

bool Commands::KickCommand::execute(const std::vector<std::string>& args) {
	Logger logger = MinecraftServer::get().getLogger();
	if (args.size() < 1) {
		logger.info(RED "You didn't specify a player! You can use an '*' to kick all players!");
		return true;
	}
	
	std::string player = args[0];
	std::string reason = "Kicked by an operator";
	if (args.size() > 1) {
		reason = "";
		std::vector<std::string> newArgs = args;
		newArgs.erase(newArgs.begin());
		for (std::string& s : newArgs) reason += s + " ";
		reason.pop_back();
	}

	if (player == "*") {
		for (Player& p : MinecraftServer::get().getOnlinePlayers())
			p.kick({reason});

		logger.info("Kicked all players for reason: '" + reason + "'!");
		return true;
	}

	Player* p = MinecraftServer::get().getPlayerByName(player);
	if (p != nullptr) {
		p->kick({reason});
		logger.info("Kicked player '" + p->getName() + "' for reason: '" + reason + "'!");
	} else logger.info(RED "Player '" + player + "' is not online!");
	return true;
}
