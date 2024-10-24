//
// Created by justi on 2024-09-08.
//

#ifndef SANDBOX_ARGUMENT_PARSER_H
#define SANDBOX_ARGUMENT_PARSER_H

namespace hive {
    class ArgumentParser {
    public:
        struct Argument {
            std::string name;
            int nArgs;
            std::string type;
            std::string shortArg;
            std::string longArg;
        };
        ArgumentParser(int argc, char **argv, const std::string& prefixChar = "-", bool allowAbbrev = true);
        Argument addArgument(const std::string &name,int nargs = 0, const std::string& type = "string", const std::string& short_arg = "", const std::string& long_arg = "");
        void parseArguments();
        bool checkArgument(const Argument &arg);
        std::vector<std::string> getStringValues(const Argument &arg, bool ignoreType = false);
        std::vector<int> getIntValues(const Argument &arg);
        std::vector<float> getFloatValues(const Argument &arg);
    private:
        int argc_;
        char **argv_;
        std::string prog_;
        std::string prefix_char_;
        bool allowAbbrev_;
        std::vector<Argument> arguments_;
        std::unordered_map<std::string, std::vector<std::string>> parsedArguments_;
    };
} // hive

#endif // SANDBOX_ARGUMENT_PARSER_H
