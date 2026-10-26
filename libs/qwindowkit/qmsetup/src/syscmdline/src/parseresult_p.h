#ifndef PARSERESULT_P_H
#define PARSERESULT_P_H

#include "sharedbase_p.h"
#include "parser.h"

#include "map_p.h"

namespace SysCmdLine {

    struct ArgumentHolderData {
        int optionalArgIndex;
        int multiValueArgIndex;
        GenericMap argNameIndexes; 
        int argSize;               

        ArgumentHolderData() : optionalArgIndex(-1), multiValueArgIndex(-1), argSize(0) {
        }
    };

    struct OptionData : public ArgumentHolderData {
        const Option *option;           
        std::vector<Value> **argResult; 
        int count;                      

        OptionData() : option(nullptr), argResult(nullptr), count(0) {
        }

        ~OptionData() {
            for (int i = 0; i < count; ++i) {
                delete[] argResult[i];
            }
            delete[] argResult;
        }

        static const OptionData &sharedNull();
    };

    struct ParseResultData2 : public ArgumentHolderData {
        std::vector<Value> *argResult;    

        OptionData *allOptionsResult;     
        int allOptionsSize;               
        int globalOptionsSize;            
        GenericMap allOptionTokenIndexes; 

        GenericMap cmdNameIndexes;        

        ~ParseResultData2() {
            delete[] allOptionsResult;
            delete[] argResult;
        }
    };

    class ParseResultPrivate : public SharedBasePrivate {
    public:
        ParseResultPrivate()
            : error(ParseResult::NoError), errorOption(nullptr), errorArgument(nullptr) {
        }

        ParseResultPrivate(const ParseResultPrivate &) = delete;
        ParseResultPrivate &operator=(const ParseResultPrivate &) = delete;

        
        
        SharedBasePrivate *clone() const {
            return nullptr;
        }

        
        Parser parser;
        std::vector<std::string> arguments;

        
        ParseResult::Error error;
        std::vector<std::string> errorPlaceholders;
        const Option *errorOption;
        const Argument *errorArgument;
        std::string cancellationToken;

        
        std::vector<int> stack;
        const Command *command; 

        ParseResultData2 core;

        bool roleSet[5] = {};

        std::string correctionText() const;

        std::vector<Option> globalOptions() const;

        
        void showMessage(const std::string &info, const std::string &catalogNames,
                         const std::string &symbolIndexes, bool isMsg = false) const;
    };

}

#endif 
