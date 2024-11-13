//
// Created by justi on 2024-09-08.
//

#include "argument_parser.h"
#include <filesystem>

namespace hive {
    ArgumentParser::ArgumentParser(int argc, char **argv, const std::string& prefixChar, bool allowAbbrev)
            : argc_(argc), argv_(argv), prefix_char_(prefixChar), allowAbbrev_(allowAbbrev) {

        if(prefixChar.length() != 1) {
            throw std::invalid_argument("Prefix character must be a single character (" + prog_ + ").");
        }

        prog_ = std::filesystem::path(argv_[0]).filename().string();
    }

    ArgumentParser::Argument ArgumentParser::addArgument(const std::string &name, int nargs, const std::string& type, const std::string& short_arg,
                                                         const std::string& long_arg) {
        if (name.empty()) {
            throw std::invalid_argument("Cannot add Argument without a name (" + prog_ + ").");
        }

        std::string actual_short_arg = short_arg.empty() ? std::string(1, name[0]) : short_arg;
        std::string actual_long_arg = long_arg.empty() ? name : long_arg;

        for (const auto& arg : arguments_) {
            if (arg.name == name) {
                throw std::invalid_argument("Cannot add duplicate argument '" + name + "' (parser of " + prog_ + ").");
            }
            if (arg.shortArg == actual_short_arg) {
                throw std::invalid_argument("Cannot add duplicate short argument '" + actual_short_arg + "' to parser of " + prog_ + ".");
            }
            if (arg.longArg == actual_long_arg) {
                throw std::invalid_argument("Cannot add duplicate long argument '" + actual_long_arg + "' to parser of " + prog_ + ".");
            }
        }
        Argument arg{name,nargs, type,actual_short_arg,actual_long_arg};
        arguments_.push_back(arg);

        return arg;
    }

    void ArgumentParser::parseArguments() {
        enum class ParseState {
            Start,
            Argument,
            Value
        };

        ParseState currentState = ParseState::Start;
        int argIndex = 1;
        Argument previousArg;
        bool foundArg = false;
        int valuesLeft = 0;


        while (argIndex < argc_) {
            switch (currentState) {
                case ParseState::Start:
                    if (argv_[argIndex][0] == prefix_char_[0]) {
                        if (valuesLeft > 0) {
                            throw std::out_of_range("Too few arguments provided for -" + previousArg.name + ". Missing " + std::to_string(valuesLeft) + " arguments (" + prog_ + ").");
                        }
                        currentState = ParseState::Argument;
                    } else {
                        currentState = ParseState::Value;
                        valuesLeft--;
                    }
                    break;
                case ParseState::Argument:
                    for(const Argument& arg : arguments_) {
                        if ((std::string(argv_[argIndex]) == prefix_char_[0] + arg.shortArg and allowAbbrev_) or std::string(argv_[argIndex]) == prefix_char_ + prefix_char_ + arg.longArg) {
                            parsedArguments_.emplace(arg.name, std::vector<std::string>{});
                            previousArg = arg;
                            valuesLeft = arg.nArgs;
                            foundArg = true;
                        }
                    }
                    if (!foundArg) {
                        std::string currentArg = std::string(argv_[argIndex]);
                        if (currentArg[0] == prefix_char_[0] && currentArg[1] != prefix_char_[0] && currentArg.length() > 2) {
                            throw std::invalid_argument("Invalid short argument: " + currentArg + " (" + prog_ + "). Short arguments must be a single character after a single '-'.");
                        }
                        throw std::invalid_argument("Unrecognized argument: " + currentArg + " (" + prog_ + ").");
                    }
                    foundArg = false;
                    currentState = ParseState::Start;
                    ++argIndex;
                    break;
                case ParseState::Value:
                    if (previousArg.name.empty()) {
                        throw std::invalid_argument("Value provided without an argument (" + prog_ + ")." );
                    }

                    if (previousArg.nArgs == 0) {
                        throw std::out_of_range("Argument -" + previousArg.name + " does not take any values (" + prog_ + ").");
                    }

                    if (parsedArguments_[previousArg.name].size() >= previousArg.nArgs) {
                        throw std::out_of_range("Too many arguments provided for -" + previousArg.name + " (" + prog_ + ").");
                    }

                    parsedArguments_[previousArg.name].emplace_back(argv_[argIndex]);
                    currentState = ParseState::Start;
                    ++argIndex;
                    break;
            }
        }
        if (valuesLeft > 0) {
            throw std::out_of_range("Too few arguments provided for -" + previousArg.name + ". Missing " + std::to_string(valuesLeft) + " arguments (" + prog_ + ").");
        }
    }

    bool ArgumentParser::checkArgument(const ArgumentParser::Argument &arg) {
        return parsedArguments_.find(arg.name) != parsedArguments_.end();
    }

    std::vector<std::string> ArgumentParser::getStringValues(const ArgumentParser::Argument &arg, bool ignoreType) {
        if (parsedArguments_.find(arg.name) != parsedArguments_.end()) {
            if (arg.type == "string" or ignoreType) {
                return parsedArguments_[arg.name];
            } else {
                throw std::invalid_argument("Argument " + arg.name + " is not of type string (" + prog_ + ").");
            }
        }
        return {};
    }

    std::vector<int> ArgumentParser::getIntValues(const ArgumentParser::Argument &arg) {
        std::vector<int> values;
        if (arg.type != "int") {
            throw std::invalid_argument("Argument " + arg.name + " is not of type int (" + prog_ + ").");
        }
        for (const auto& value : getStringValues(arg, true)) {
            values.push_back(std::stoi(value));
        }
        return values;
    }

    std::vector<float> ArgumentParser::getFloatValues(const ArgumentParser::Argument &arg) {
        std::vector<float> values;
        if (arg.type != "float") {
            throw std::invalid_argument("Argument " + arg.name + " is not of type float (" + prog_ + ").");
        }
        for (const auto& value : getStringValues(arg, true)) {
            values.push_back(std::stof(value));
        }
        return values;
    }
} // hive
