#pragma once

#include <vector>
#include <unordered_map>
#include "../../io/logger.h"

namespace Commands {
	class Command {
	public:
		Command(const std::string& name) : name(name), desc("None given."), usage("None given."), aliases({name}) {}
		Command(const std::string& name, const std::string& description, const std::string& usage, const std::vector<std::string>& a) : name(name), desc(description), usage(usage), aliases(a) {
			aliases.push_back(name);
		}

		virtual ~Command() {}

		virtual bool compare(const std::string& input) const {
			for (const std::string& alias : aliases) {
				if (input.find(alias) == 0) return true;
			}
			return false;
		}

		virtual void printData();

		virtual bool execute(const std::vector<std::string>& args) = 0;

	private:
		std::string name;
		std::string desc;
		std::string usage;
		std::vector<std::string> aliases;
	};

	class StopCommand : public Command {
	public:
		StopCommand() : Command("stop", "Stops the server.", "/stop", {"exit", "quit"}) {}

		bool execute(const std::vector<std::string>& args) override;
	};

	class ReloadCommand : public Command {
	public:
		ReloadCommand() : Command("config", "Manages the server's configs.", "/config <option>", {}) {}

		bool execute(const std::vector<std::string>& args) override;
	};

	class HelpCommand : public Command {
	public:
		HelpCommand() : Command("help", "Displays the list of commands.", "/help", {"?"}) {}

		bool execute(const std::vector<std::string>& args) override;
	};

	class KickCommand : public Command {
	public:
		KickCommand() : Command("kick", "Kicks the selected player from the server.", "/kick <player> [<reason>]", {}) {}

		bool execute(const std::vector<std::string>& args) override;
	};
};