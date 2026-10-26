#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <set>
#include <stdexcept>
#include <iomanip>
#include <map>

#include <syscmdline/parser.h>
#include <syscmdline/system.h>

#include "sha-256.h"
#include "utils.h"

namespace SCL = SysCmdLine;

namespace fs = std::filesystem;

using SCL::u8info;



#ifdef _WIN32
#  define OS_EXECUTABLE "Windows PE"
#elif defined(__APPLE__)
#  define OS_EXECUTABLE "Mach-O"
#else
#  define OS_EXECUTABLE "ELF"
#endif

#ifdef _WIN32
using TChar = wchar_t;
using TString = std::wstring;
#else
using TChar = char;
using TString = std::string;
#endif

using TStringList = std::vector<TString>;
using TStringSet = std::set<TString>;

#ifdef _WIN32
#  define _TSTR(X) L##X
#  define tstrcmp  wcscmp
#else
#  define _TSTR(X) X
#  define tstrcmp  strcmp
#endif



static inline std::string tstr2str(const TString &str) {
#ifdef _WIN32
    return SCL::wideToUtf8(str);
#else
    return str;
#endif
}

static inline TString str2tstr(const std::string &str) {
#ifdef _WIN32
    return SCL::utf8ToWide(str);
#else
    return str;
#endif
}

static std::string time2str(const std::chrono::system_clock::time_point &t) {
    std::time_t t2 = std::chrono::system_clock::to_time_t(t);
    std::string s(30, '\0');
    std::strftime(s.data(), s.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&t2));
    return s;
}

template <class T>
static bool searchInRegexList(std::basic_string<T> s,
                              const std::vector<std::basic_string<T>> &regexList) {
    
    std::replace(s.begin(), s.end(), _TSTR('\\'), _TSTR('/'));

    bool found = false;
    for (const auto &pattern : regexList) {
        if (std::regex_search(s.begin(), s.end(), std::basic_regex<T>(pattern))) {
            found = true;
            break;
        }
    }
    return found;
}

static bool removeEmptyDirectories(const fs::path &path, bool verbose) {
    bool isEmpty = true;
    for (const auto &entry : fs::directory_iterator(path)) {
        if (fs::is_directory(entry.path()) && removeEmptyDirectories(entry.path(), verbose)) {
            continue;
        }

        
        isEmpty = false;
    }

    
    if (isEmpty) {
        if (verbose) {
            u8info("Remove: \"%s\"\n", tstr2str(path).data());
        }
        fs::remove(path);
    }

    
    return isEmpty;
}

static bool copyFile(const fs::path &file, const fs::path &dest, const fs::path &symlinkContent,
                     bool force, bool verbose) {
    auto target = dest / file.filename();
    if (fs::exists(target)) {
        if (Utils::cleanPath(target) == Utils::cleanPath(file))
            return false; 

        if (!force && Utils::fileTime(target).modifyTime >= Utils::fileTime(file).modifyTime)
            return false; 
    } else if (!fs::is_directory(dest)) {
        fs::create_directories(dest);
    }

    if (!symlinkContent.empty()) {
        if (verbose) {
            u8info("Link: from \"%s\" to \"%s\"\n", tstr2str(file).data(),
                   tstr2str(symlinkContent).data());
        }

        if (fs::exists(target))
            fs::remove(target);
        fs::create_symlink(symlinkContent, target);
    } else {
        if (verbose) {
            u8info("Copy: from \"%s\" to \"%s\"\n", tstr2str(file).data(), tstr2str(target).data());
        }
        fs::copy(file, dest, fs::copy_options::overwrite_existing);
        Utils::syncFileTime(target, file); 
    }

    return true;
}

static void copyDirectory(const fs::path &srcRootDir, const fs::path &srcDir,
                          const fs::path &destDir, bool force, bool verbose,
                          const std::function<bool(const fs::path &)> &ignore = {}) {
    fs::create_directories(destDir); 

    for (const auto &entry : fs::directory_iterator(srcDir)) {
        const auto &entryPath = entry.path();
        if (ignore && ignore(entryPath))
            continue;

#ifndef _WIN32
        if (fs::is_symlink(entryPath)) {
            fs::path linkPath;
            try {
                linkPath = fs::canonical(entryPath);
            } catch (...) {
                
                copyFile(entryPath, destDir, {}, force, verbose);
                continue;
            }

            
            copyFile(entryPath, destDir,
                     Utils::starts_with(linkPath.string(), srcRootDir.string())
                         ? fs::relative(linkPath, fs::canonical(entryPath.parent_path())).string()
                         : std::string(),
                     force, verbose);
            continue;
        }
#endif

        if (fs::is_regular_file(entryPath)) {
            copyFile(entryPath, destDir, {}, force, verbose);
        } else if (fs::is_directory(entryPath)) {
            copyDirectory(srcRootDir, entryPath, destDir / entryPath.filename(), force, verbose,
                          ignore);
        }
    }
}

static std::string standardError(int code = errno) {
    return std::error_code(code, std::generic_category()).message();
}

#ifdef __APPLE__
static inline bool isFramework(const fs::path &path) {
    return Utils::toLower(path.extension().string()) == ".framework";
}

static fs::path lib2framework(fs::path path, const fs::path &fallback = {}) {
    
    for (int i = 0; i < 3; ++i) {
        if (!path.has_parent_path())
            return fallback;
        path = path.parent_path();
    }
    
    if (isFramework(path))
        return path;
    return fallback;
}

static fs::path framework2lib(fs::path path, const fs::path &fallback = {}) {
    try {
        return fs::canonical(path / path.stem());
    } catch (...) {
    }
    return fallback;
}

static fs::path framework2lib_debug(fs::path path, const fs::path &fallback = {}) {
    try {
        return fs::canonical(path / (path.stem().string() + "_debug"));
    } catch (...) {
    }
    return fallback;
}
#endif

#ifdef _WIN32
static inline bool isMSVCLibrary(const TString &fileName) {
    return Utils::starts_with(fileName, _TSTR("vcruntime")) ||
           Utils::starts_with(fileName, _TSTR("msvcp")) ||
           Utils::starts_with(fileName, _TSTR("concrt")) ||
           Utils::starts_with(fileName, _TSTR("vccorlib")) ||
           Utils::starts_with(fileName, _TSTR("ucrtbase"));
}

static inline fs::path searchWindowsSystemPaths(const TString &fileName) {
    static std::vector<const wchar_t *> searchPaths = []() -> std::vector<const wchar_t *> {
        static const wchar_t sys[] = L"C:\\Windows";
        static const wchar_t sys32[] = L"C:\\Windows\\System32";

#  if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__)
        return {
            sys,
            sys32,
        };
#  else
        static const wchar_t syswow64[] = L"C:\\Windows\\SysWOW64";
        if (fs::exists(syswow64)) {
            return {
                sys,
                syswow64,
            };
        }
        return {
            sys,
            sys32,
        };
#  endif
    }();

    for (const auto &path : std::as_const(searchPaths)) {
        auto fullPath = fs::path(path) / fileName;
        if (fs::exists(fullPath)) {
            return fullPath;
        }
    }
    return {};
}
#else
static fs::path copyCanonical(const fs::path &path, const fs::path &dest, bool force,
                              bool verbose) {
    
    fs::path target;
    if (fs::is_symlink(path)) {
        auto linkPath = fs::canonical(path);
        copyFile(linkPath, dest, {}, force, verbose);              
        copyFile(path, dest, linkPath.filename(), force, verbose); 
        target = dest / linkPath.filename();
    } else {
        copyFile(path, dest, {}, force, verbose); 
        target = dest / path.filename();
    }
    return target;
};
#endif

static inline fs::path toFramework(const fs::path &path) {
    return
#ifdef __APPLE__
        lib2framework(path, path)
#else
        path
#endif
            ;
}

static inline fs::path fromFramework(const fs::path &path) {
#ifdef __APPLE__
    auto res = framework2lib(path);
    if (res.empty()) {
        res = framework2lib_debug(path);
        if (res.empty()) {
            res = path;
        }
    }
    return res;
#else
    return path;
#endif
}



static inline bool isForceSet(const SCL::ParseResult &result) {
    return result.isOptionSet("-f");
}

static inline bool isDryRunSet(const SCL::ParseResult &result) {
    return result.isOptionSet("-d");
}

static inline bool isVerboseSet(const SCL::ParseResult &result) {
    return result.isRoleSet(SCL::Option::Verbose);
}

static inline bool isStandardSet(const SCL::ParseResult &result) {
    return result.isOptionSet("-s");
}

static int cmd_copy(const SCL::ParseResult &result) {
    bool force = isForceSet(result);
    bool verbose = isVerboseSet(result);

    std::set<fs::path> files;
    std::set<fs::path> directories;
    std::set<fs::path> directoryContents;
    {
        const auto &srcResult = result.values(0);
        for (const auto &item : srcResult) {
            const auto &rawString = item.toString();
            bool contents = false;
            if (auto last = rawString.back(); last == '/' || last == '\\') {
                contents = true;
            }

            const auto &path = Utils::cleanPath(fs::absolute(str2tstr(rawString)));
            if (fs::is_directory(path)) {
                (contents ? directoryContents : directories).insert(path);
            } else if (fs::exists(path)) {
                files.insert(path);
            } else {
                throw std::runtime_error("not a file or directory: \"" + rawString + "\"");
            }
        }
    }

    const auto &dest = Utils::cleanPath(fs::absolute(str2tstr(result.value(1).toString())));

    
    TStringList excludes;
    {
        const auto &excludeResult = result.option("-e").allValues();
        excludes.reserve(excludeResult.size());
        for (const auto &item : excludeResult) {
            excludes.emplace_back(str2tstr(item.toString()));
        }
    }

    
    const auto &excludeFunc = [&excludes](const fs::path &path) {
        return searchInRegexList(TString(path), excludes);
    };

    for (const auto &item : std::as_const(files)) {
        copyFile(item, dest, {}, force, verbose);
    }
    for (const auto &item : std::as_const(directories)) {
        copyDirectory(item, item, dest / item.filename(), force, verbose, excludeFunc);
    }
    for (const auto &item : std::as_const(directoryContents)) {
        copyDirectory(item, item, dest, force, verbose, excludeFunc);
    }

    return 0;
}

static int cmd_rmdir(const SCL::ParseResult &result) {
    bool verbose = isVerboseSet(result);

    std::vector<fs::path> dirs;
    {
        const auto &dirsResult = result.values(0);
        dirs.reserve(dirsResult.size());
        for (const auto &item : dirsResult) {
            dirs.emplace_back(fs::absolute(str2tstr(item.toString())));
        }
    }

    for (const auto &item : std::as_const(dirs)) {
        if (!fs::is_directory(item)) {
            continue;
        }
        removeEmptyDirectories(item, verbose);
    }
    return 0;
}

static int cmd_touch(const SCL::ParseResult &result) {
    bool verbose = isVerboseSet(result);

    const auto &file = str2tstr(result.value(0).toString());
    const auto &refFile = str2tstr(result.value(1).toString());

    
    if (!fs::is_regular_file(file)) {
        throw std::runtime_error("not a regular file: \"" + tstr2str(file) + "\"");
    }

    if (!refFile.empty() && !fs::is_regular_file(refFile)) {
        throw std::runtime_error("not a regular file: \"" + tstr2str(refFile) + "\"");
    }

    
    Utils::FileTime t;
    if (!refFile.empty()) {
        t = Utils::fileTime(refFile);
    } else {
        auto now = std::chrono::system_clock::now();
        t = {now, now, now};
    }

    
    if (verbose) {
        u8info("Set A-Time: %s\n", time2str(t.accessTime).data());
        u8info("Set M-Time: %s\n", time2str(t.modifyTime).data());
        u8info("Set C-Time: %s\n", time2str(t.statusChangeTime).data());
    }
    Utils::setFileTime(file, t);
    return 0;
}

static int cmd_configure(const SCL::ParseResult &result) {
    bool dryrun = isDryRunSet(result);
    bool force = isForceSet(result);
    bool verbose = dryrun || isVerboseSet(result);

    const auto &projectName = result.valueForOption("-p").toString();
    const auto &fileName = fs::absolute(str2tstr(result.value(0).toString()));

    
    std::vector<std::string> warningLines;
    if (const auto &warningResult = result.option("-w"); warningResult.isSet()) {
        do {
            const auto &warningFileString = warningResult.value(0).toString();
            if (warningFileString.empty())
                break;

            const auto &warningFile = str2tstr(warningFileString);
            std::ifstream inFile((fs::path(warningFile)));
            if (!inFile.is_open()) {
                break;
            }

            std::string line;
            while (std::getline(inFile, line)) {
                line = Utils::trim(line);
                if (line.empty())
                    continue;
                warningLines.push_back(line);
            }

            inFile.close();
        } while (false);

        if (warningLines.empty()) {
            warningLines = {
                "Caution: This file is generated by CMake automatically during configure.",
                "WARNING!!! DO NOT EDIT THIS FILE MANUALLY!!!",
                "ALL YOUR MODIFICATIONS HERE WILL GET LOST AFTER RE-CONFIGURING!!!",
            };
        }
    }

    
    std::vector<std::string> defines;
    {
        const auto &definesResult = result.option("-D").allValues();
        defines.reserve(definesResult.size());
        for (const auto &item : definesResult) {
            defines.emplace_back(item.toString());
        }
    }

    
    std::string definitions;
    {
        std::map<std::string, size_t> definitionMap;
        std::vector<std::pair<std::string, std::string>> definitionSequence; 
        for (const auto &def : std::as_const(defines)) {
            
            if (def.front() == '%') {
                definitionSequence.push_back(std::make_pair("%", def.substr(1)));
                continue;
            }

            std::string key;
            std::string val;
            size_t pos = def.find('=');
            if (pos == std::string::npos) {
                key = def;
            } else {
                key = def.substr(0, pos);
                val = def.substr(pos + 1);
            }

            auto it = definitionMap.find(key);
            if (it == definitionMap.end()) {
                definitionMap[key] = definitionSequence.size();
                definitionSequence.push_back(std::make_pair(key, val));
            } else {
                definitionSequence[it->second].second = val;
            }
        }

        std::stringstream ss;
        for (const auto &pair : std::as_const(definitionSequence)) {
            if (pair.first == "%") {
                ss << pair.second << "\n";
            } else if (pair.second.empty()) {
                ss << "#define " << pair.first << "\n";
            } else {
                ss << "#define " << pair.first << " " << pair.second << "\n";
            }
        }
        definitions = ss.str();
    }

    
    std::string hash;
    if (!force) {
        uint8_t buf[32];
        calc_sha_256(buf, definitions.data(), definitions.size());

        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (auto byte : buf) {
            ss << std::setw(2) << static_cast<int>(byte);
        }
        hash = ss.str();
    }

    
    std::string content;
    {
        std::string guard = tstr2str(fileName.filename());
        {
            
            if (!projectName.empty()) {
                guard = projectName + "_" + guard;
            }

            
            std::replace_if(
                guard.begin(), guard.end(), [](const char &c) { return !isalnum(c) && c != '_'; },
                '_');

            
            if (std::isdigit(guard.front())) {
                guard = "_" + guard;
            }

            
            guard = Utils::toUpper(guard);
        }

        std::stringstream ss;

        
        ss << "#ifndef " << guard << "\n";
        ss << "#define " << guard << "\n\n";

        
        if (!warningLines.empty()) {
            for (const auto &item : std::as_const(warningLines)) {
                ss << "// " << item << "\n";
            }
            ss << "\n";
        }

        
        if (!hash.empty()) {
            ss << "// SHA256: " << hash << "\n\n";
        }

        
        ss << definitions << "\n";

        
        ss << "#endif // " << guard << "\n";

        content = ss.str();
    }

    if (dryrun) {
        u8info("%s", content.data());
        return 0;
    }

    
    do {
        if (force)
            break;

        
        std::ifstream inFile(fileName);
        if (!inFile.is_open()) {
            break;
        }

        bool matched = false;

        static std::regex hashPattern(R"(^// SHA256: (\w+)$)");
        std::smatch match;
        std::string line;

        int pp_cnt = 0;
        while (std::getline(inFile, line)) {
            if (line.empty())
                continue;

            if (line.front() == '#') {
                pp_cnt++; 
                if (pp_cnt > 2) {
                    break;
                }
                continue;
            }

            if (!Utils::starts_with(line, "//"))
                break;

            if (std::regex_match(line, match, hashPattern)) {
                if (match[1] == hash) {
                    matched = true;
                }
                break;
            }
        }
        inFile.close();

        if (matched) {
            if (verbose) {
                SCL::u8debug(SCL::MessageType::MT_Warning, true, "Content matched. (%s)\n",
                             hash.data());
            }
            
            return 0;
        }
    } while (false);

    
    {
        
        if (auto dir = fileName.parent_path(); !fs::is_directory(dir)) {
            fs::create_directories(dir);
        }

        std::ofstream outFile(fileName);
        if (!outFile.is_open()) {
            throw std::runtime_error("failed to open file \"" + tstr2str(fileName) +
                                     "\": " + standardError());
        }

        outFile << content;
        outFile.close();

        if (verbose) {
            if (!hash.empty()) {
                u8info("SHA256: %s\n", hash.data());
            }
        }
    }

    return 0;
}

static int cmd_incsync(const SCL::ParseResult &result) {
    bool dryrun = isDryRunSet(result);
    bool verbose = dryrun || isVerboseSet(result);
    bool force = isForceSet(result);
    bool standard = isStandardSet(result);
    bool copy = result.isOptionSet("-c");
    bool all = !result.isOptionSet("-n");

    const fs::path &src = Utils::cleanPath(fs::absolute(str2tstr(result.value(0).toString())));
    const fs::path &dest = Utils::cleanPath(fs::absolute(str2tstr(result.value(1).toString())));
    if (!fs::is_directory(src)) {
        throw std::runtime_error("not a directory: \"" + tstr2str(src) + "\"");
    }

    
    std::vector<std::pair<TString, TString>> includes;
    {
        const auto &includeResult = result.option("-i");
        int cnt = includeResult.count();
        includes.reserve(cnt + 1);

        
        if (standard) {
            includes.emplace_back(_TSTR(R"(.*?_p\..+$)"), _TSTR("private"));
        }

        for (int i = 0; i < cnt; ++i) {
            includes.emplace_back(str2tstr(includeResult.value(0, i).toString()),
                                  str2tstr(includeResult.value(1, i).toString()));
        }
    }

    
    TStringList excludes;
    {
        const auto &excludeResult = result.option("-e").allValues();
        excludes.reserve(excludeResult.size());
        for (const auto &item : excludeResult) {
            excludes.emplace_back(str2tstr(item.toString()));
        }
    }

    
    if (!dryrun && fs::exists(dest) && force) {
        fs::remove_all(dest);
    }

    for (const auto &entry : fs::recursive_directory_iterator(src)) {
        if (entry.is_regular_file()) {
            const auto &path = entry.path();
            const auto &ext = Utils::toLower(TString(path.extension()));
            if (!(ext == _TSTR(".h") || ext == _TSTR(".hh") || ext == _TSTR(".hpp") ||
                  ext == _TSTR(".hxx"))) {
                continue;
            }

            
            fs::path subdir;
            for (const auto &pair : includes) {
                TString pathString = path;
                
                std::replace(pathString.begin(), pathString.end(), _TSTR('\\'), _TSTR('/'));

                if (std::regex_search(pathString.begin(), pathString.end(),
                                      std::basic_regex<TChar>(pair.first))) {
                    subdir = pair.second;
                }
            }

            if (!all && subdir.empty())
                continue;

            
            if (searchInRegexList(TString(path), excludes))
                continue;

            const fs::path &targetDir = subdir.empty() ? dest : (dest / subdir);

            auto targetPath = targetDir / path.filename();
            if (verbose) {
                u8info("Sync: from \"%s\" to \"%s\"\n", tstr2str(path).data(),
                       tstr2str(targetPath).data());
            }

            if (dryrun)
                continue;

            if (fs::exists(targetPath) &&
                Utils::fileTime(targetPath).modifyTime >= Utils::fileTime(path).modifyTime) {
                continue;
            }

            
            if (!fs::exists(targetDir)) {
                fs::create_directories(targetDir);
            }

            if (copy) {
                
                fs::copy(path, targetPath, fs::copy_options::overwrite_existing);
            } else {
                
                std::string rel = tstr2str(fs::relative(path, targetDir));

#ifdef _WIN32
                
                std::replace(rel.begin(), rel.end(), '\\', '/');
#endif

                
                std::ofstream outFile(targetPath);
                if (!outFile.is_open()) {
                    throw std::runtime_error("failed to open file \"" + tstr2str(targetPath) +
                                             "\": " + standardError());
                }
                outFile << "#include \"" << rel << "\"" << std::endl;
                outFile.close();
            }

            
            Utils::syncFileTime(targetPath, path);
        }
    }

    return 0;
}

static int cmd_deploy(const SCL::ParseResult &result) {
    bool dryrun = isDryRunSet(result);
    bool verbose = dryrun || isVerboseSet(result);
    bool force = isForceSet(result);
    bool standard = isStandardSet(result);

    fs::path dest = fs::current_path(); 
    if (auto destResult = result.option("-o"); destResult.isSet()) {
        dest = Utils::cleanPath(fs::absolute(str2tstr(destResult.value().toString())));
    }

    
    std::set<fs::path> orgFiles;
    {
        const auto &files = result.values(0);
        for (const auto &item : files) {
            orgFiles.insert(toFramework(Utils::cleanPath(fs::absolute(str2tstr(item.toString())))));
        }
    }

    
    std::vector<std::pair<fs::path, fs::path>> extraOrgFiles;
    {
        const auto &copiesResult = result.option("-c");
        int cnt = copiesResult.count();
        extraOrgFiles.reserve(cnt);
        for (int i = 0; i < cnt; ++i) {
            extraOrgFiles.emplace_back(
                toFramework(
                    Utils::cleanPath(fs::absolute(str2tstr(copiesResult.value(0, i).toString())))),
                Utils::cleanPath(fs::absolute(str2tstr(copiesResult.value(1, i).toString()))));
        }
    }

#if 1
    
    std::vector<fs::path> searchingPaths;
    {
        const auto &linkResult = result.option("-L").allValues();

        std::vector<fs::path> tmp;
        tmp.reserve(linkResult.size() + orgFiles.size());

        
        for (const auto &item : std::as_const(orgFiles)) {
            tmp.emplace_back(fs::path(item).parent_path());
        }

        
        for (const auto &item : linkResult) {
            tmp.emplace_back(Utils::cleanPath(fs::absolute(str2tstr(item.toString()))));
        }

        
        const auto &configFiles = result.option("-@").allValues();
        for (const auto &item : configFiles) {
            std::ifstream file(fs::path(str2tstr(item.toString())), std::ios::binary);
            if (!file.is_open()) {
                continue;
            }

            
            char bom[3];
            file.read(bom, 3);
            if (file.gcount() != 3) {
                continue;
            }
            if (!(bom[0] == (char) 0xEF && bom[1] == (char) 0xBB && bom[2] == (char) 0xBF)) {
                file.seekg(0);
            }

            std::string line;
            while (std::getline(file, line)) {
                if (line.empty())
                    continue;
                tmp.emplace_back(Utils::cleanPath(fs::absolute(str2tstr(Utils::trim(line)))));
            }
        }

        
        tmp.push_back(dest);

        
        std::set<fs::path> visited;
        for (auto item : std::as_const(tmp)) {
            if (fs::is_regular_file(item))
                item = item.parent_path();
            else if (!fs::is_directory(item))
                continue;

            if (Utils::contains(visited, item)) {
                continue;
            }
            visited.insert(item);
            searchingPaths.emplace_back(item);
        }
    }
#endif

    
    TStringList excludes;
    {
        const auto &excludeResult = result.option("-e").allValues();
        excludes.reserve(excludeResult.size());
        for (const auto &item : excludeResult) {
            excludes.emplace_back(str2tstr(item.toString()));
        }
    }

    
    std::vector<fs::path> dependencies;
#ifdef __APPLE__
    enum FrameworkType {
        NoFramework = 0,
        Release = 1,
        Debug = 2,
    };
    std::map<std::string, int> depFrameworkTypes;
#endif
    {
        std::set<fs::path> allOrgFileNames;
        for (const auto &item : std::as_const(orgFiles)) {
            allOrgFileNames.insert(item.filename());
        }
        for (const auto &pair : std::as_const(extraOrgFiles)) {
            allOrgFileNames.insert(pair.first.filename());
        }

        const auto &getFilesDeps =
            [&](const std::vector<fs::path> &paths) -> std::vector<fs::path> {
            std::set<TString> result;
            for (const auto &path : std::as_const(paths)) {
                if (verbose) {
                    u8info("Resolve: \"%s\"\n", tstr2str(path).data());
                }

                std::vector<std::string> unparsed;
                const auto &deps =
#ifdef _WIN32
                    Utils::resolveWinBinaryDependencies(path, searchingPaths, &unparsed)
#else
                    Utils::resolveUnixBinaryDependencies(fromFramework(path), searchingPaths,
                                                         &unparsed)
#endif
                    ;
                for (const auto &item : deps) {
                    if (verbose) {
                        u8info("    %s\n", tstr2str(item).data());
                    }
                    result.insert(item);
                }

                if (verbose) {
                    size_t maxSize = 0;
                    for (const auto &item : std::as_const(unparsed)) {
                        maxSize = std::max(maxSize, item.size());
                    }
                    for (const auto &item : std::as_const(unparsed)) {
                        u8info("    %s%s[Not Found]\n", item.data(),
                               std::string(maxSize + 4 - item.size(), ' ').data());
                    }
                }
            }
            return {result.begin(), result.end()};
        };

        std::set<fs::path> visited;
        std::vector<fs::path> stack = {orgFiles.begin(), orgFiles.end()};

        
        for (const auto &item : std::as_const(orgFiles)) {
            visited.insert(item.filename());
        }
        for (const auto &pair : std::as_const(extraOrgFiles)) {
            visited.insert(pair.first.filename());
            stack.push_back(pair.first);
        }

        
        while (!stack.empty()) {
            auto libs = getFilesDeps(stack);
            stack.clear();

            
            for (const auto &lib : std::as_const(libs)) {
                const auto &path = toFramework(lib);
                
                if (Utils::contains(allOrgFileNames, path.filename()))
                    continue;

                
                TString fileName = Utils::toLower(TString(path.filename()));
                if ((standard && (
#ifdef _WIN32
                                     isMSVCLibrary(fileName)
#elif defined(__APPLE__)
                                     Utils::starts_with(fileName, "libc++") ||
                                     Utils::starts_with(fileName, "libSystem") ||
                                     Utils::starts_with(fileName, "/System")
#else
                                     Utils::starts_with(fileName, "libstdc++") ||
                                     Utils::starts_with(fileName, "libgcc") ||
                                     Utils::starts_with(fileName, "libglib") ||
                                     Utils::starts_with(fileName, "libpthread") ||
                                     Utils::starts_with(fileName, "libgthread") ||
                                     Utils::starts_with(fileName, "libicu") ||
                                     Utils::starts_with(fileName, "libc.so") ||
                                     Utils::starts_with(fileName, "libc-") ||
                                     Utils::starts_with(fileName, "libdl.so") ||
                                     Utils::starts_with(fileName, "libdl-")
#endif
                                         )) ||
#ifdef _WIN32
                    (!searchWindowsSystemPaths(fileName).empty() ||
                     Utils::starts_with(fileName, _TSTR("api-ms-win-")) ||
                     Utils::starts_with(fileName, _TSTR("ext-ms-win-"))) ||
#endif
                    Utils::contains(visited, fileName)) {
                    continue;
                }
                visited.insert(fileName);

                
                if (searchInRegexList(TString(path), excludes))
                    continue;

#ifdef __APPLE__
                
                if (fs::is_directory(path)) {
                    std::string name = lib.filename();
                    if (name.ends_with("_debug")) {
                        depFrameworkTypes[path.stem()] |= Debug;
                    } else {
                        depFrameworkTypes[path.stem()] |= Release;
                    }
                }
#endif

                dependencies.push_back(path);

                
                stack.push_back(path);
            }
        }
    }

    
    if (dryrun) {
        return 0;
    }

#ifdef _WIN32
    
    

    
    for (const auto &pair : std::as_const(extraOrgFiles)) {
        copyFile(pair.first, pair.second, {}, force, verbose);
    }

    
    for (const auto &file : std::as_const(dependencies)) {
        copyFile(file, dest, {}, force, verbose);
    }
#else
    
    

    const auto &fixRPaths = [verbose](const std::string &file,
                                      const std::vector<std::string> &paths) {
        if (verbose) {
            u8info("Fix rpath: \"%s\"\n", file.data());
            for (const auto &path : paths) {
                u8info("    %s\n", path.data());
            }
        }
        Utils::setFileRPaths(file, paths);
    };

#  if defined(__APPLE__)
    
    static const auto &frameworkIgnore = [](const fs::path &path, const fs::path &frameworkName,
                                            int type) -> bool {
        if (!(type & Release)) {
            if (path.filename() == frameworkName)
                return true;
        }
        if (!(type & Debug)) {
            if (path.filename() == frameworkName.string() + "_debug")
                return true;
        }
        return fs::is_directory(path) && path.filename() == "Headers" &&
               fs::is_directory(path.parent_path() / "Resources");
    };

    
    
    const auto &fixFrameworkRPaths = [fixRPaths](const fs::path &path) {
        fixRPaths(path, {
                            "@executable_path/../Frameworks",
                            "@loader_path/Frameworks",
                            "@loader_path/../../..",
                        });
    };

    
    auto targetOrgFiles = orgFiles;
    for (const auto &pair : std::as_const(extraOrgFiles)) {
        const auto &file = pair.first;
        fs::path targetPath;
        if (fs::is_directory(file)) {
            const auto &name = file.stem();
            targetPath = pair.second / file.filename();

            copyDirectory(file, file, targetPath, force, verbose,
                          [&](const fs::path &path) -> bool {
                              return frameworkIgnore(path, name, Debug | Release); 
                          });
        } else {
            targetPath = copyCanonical(file, pair.second, force, verbose);
        }
        targetOrgFiles.insert(targetPath);
    }

    
    std::set<fs::path> targetDependencies;
    for (const auto &file : std::as_const(dependencies)) {
        fs::path targetPath;
        if (fs::is_directory(file)) {
            const auto &name = file.stem();
            targetPath = dest / file.filename();

            int type = Debug | Release;
            if (auto it = depFrameworkTypes.find(name); it != depFrameworkTypes.end()) {
                type = it->second;
            }
            copyDirectory(file, file, targetPath, force, verbose,
                          [&](const fs::path &path) -> bool {
                              return frameworkIgnore(path, name, type); 
                          });
        } else {
            targetPath = copyCanonical(file, dest, force, verbose);
        }
        targetDependencies.insert(targetPath);
    }

    
    {
        
        std::string currentArch;
        try {
            currentArch = Utils::executeCommand("uname", {"-m"});
            currentArch = Utils::trim(currentArch);
        } catch (const std::exception &e) {
            if (verbose) {
                u8info("Warning: Failed to get current architecture: %s\n", e.what());
            }
            currentArch = "unknown";
        }

        if (currentArch != "unknown") {
            const auto &stripUniversalBinary = [verbose, &currentArch](const fs::path &file) {
                if (!fs::is_regular_file(file)) {
                    return;
                }

                
                std::string lipoInfo;
                try {
                    lipoInfo = Utils::executeCommand("lipo", {"-info", file.string()});
                } catch (const std::exception &) {
                    
                    return;
                }

                
                if (lipoInfo.find("Architectures") == std::string::npos) {
                    return; 
                }

                
                if (lipoInfo.find(currentArch) == std::string::npos) {
                    if (verbose) {
                        u8info("Warning: Universal binary \"%s\" does not contain %s architecture\n",
                               file.string().data(), currentArch.data());
                    }
                    return;
                }

                
                bool lipoPassed = false;
                try {
                    if (verbose) {
                        u8info("Strip universal binary: \"%s\" (keep %s)\n",
                               file.string().data(), currentArch.data());
                    }
                    std::ignore = Utils::executeCommand("lipo", {
                        file.string(),
                        "-thin", currentArch,
                        "-output", file.string()
                    });
                    lipoPassed = true;
                } catch (const std::exception &e) {
                    if (verbose) {
                        u8info("Warning: Failed to strip universal binary \"%s\": %s\n",
                               file.string().data(), e.what());
                    }
                }
            };

            
            for (const auto &file : std::as_const(targetOrgFiles)) {
                if (isFramework(file)) {
                    fs::path lib = framework2lib(file);
                    if (!lib.empty()) {
                        stripUniversalBinary(lib);
                    }

                    fs::path libDebug = framework2lib_debug(file);
                    if (!libDebug.empty()) {
                        stripUniversalBinary(libDebug);
                    }
                } else {
                    stripUniversalBinary(file);
                }
            }

            
            for (const auto &file : std::as_const(targetDependencies)) {
                if (isFramework(file)) {
                    fs::path lib = framework2lib(file);
                    if (!lib.empty()) {
                        stripUniversalBinary(lib);
                    }

                    fs::path libDebug = framework2lib_debug(file);
                    if (!libDebug.empty()) {
                        stripUniversalBinary(libDebug);
                    }
                } else {
                    stripUniversalBinary(file);
                }
            }
        }
    }

    
    {
        
        std::set<std::string> targetFileNames;
        for (const auto &file : std::as_const(targetOrgFiles)) {
            if (isFramework(file)) {
                targetFileNames.insert(file.stem());
            } else {
                targetFileNames.insert(file.filename());
            }
        }

        for (const auto &file : std::as_const(targetDependencies)) {
            if (isFramework(file)) {
                targetFileNames.insert(file.stem());
            } else {
                targetFileNames.insert(file.filename());
            }
        }

        static const auto &normalize = [verbose, &targetFileNames](const fs::path &file) {
            std::vector<std::pair<std::string, std::string>> normalized;
            for (const auto &dep : Utils::getMacAbsoluteDependencies(
                     isFramework(file) ? framework2lib(file) : file)) {
                fs::path depPath = dep;

                
                if (fs::exists(depPath) &&
                    Utils::contains(targetFileNames, fs::canonical(depPath).filename())) {
                    normalized.emplace_back(dep, "@rpath/" + depPath.filename().string());
                }
            }

            if (normalized.empty())
                return;

            if (verbose) {
                u8info("Normalize dependencies: \"%s\"\n", file.string().data());
                for (const auto &item : std::as_const(normalized)) {
                    u8info("    %s\n", item.first.data());
                }
            }

            Utils::replaceMacFileDependencies(file, normalized);
        };

        for (const auto &file : std::as_const(targetOrgFiles)) {
            normalize(file);
        }
        for (const auto &file : std::as_const(targetDependencies)) {
            normalize(file);
        }
    }

    
    for (const auto &file : std::as_const(targetOrgFiles)) {
        if (isFramework(file)) {
            fs::path lib = framework2lib(file);
            if (!lib.empty()) {
                fixFrameworkRPaths(lib);
            }

            fs::path libDebug = framework2lib_debug(file);
            if (!libDebug.empty()) {
                fixFrameworkRPaths(libDebug);
            }
        } else {
            fixRPaths(file,
                      {
                          "@executable_path/../Frameworks",
                          "@loader_path/" +
                              Utils::cleanPath(fs::relative(dest, file.parent_path())).string(),
                      });
        }
    }

    
    for (const auto &file : std::as_const(targetDependencies)) {
        if (isFramework(file)) {
            fs::path lib = framework2lib(file);
            if (!lib.empty()) {
                fixFrameworkRPaths(lib);
            }

            fs::path libDebug = framework2lib_debug(file);
            if (!libDebug.empty()) {
                fixFrameworkRPaths(libDebug);
            }
        } else {
            fixRPaths(file, {
                                "@executable_path/../Frameworks",
                                "@loader_path",
                            });
        }
    }

#  else
    
    
    
    
    
    
    

    
    auto targetOrgFiles = orgFiles;
    for (const auto &pair : std::as_const(extraOrgFiles)) {
        auto targetPath = copyCanonical(pair.first, pair.second, force, verbose);
        targetOrgFiles.insert(targetPath);
    }

    
    std::set<fs::path> targetDependencies;
    for (const auto &file : std::as_const(dependencies)) {
        auto targetPath = copyCanonical(file, dest, force, verbose);

        
        if (targetPath.filename() != "libc.so") {
            targetDependencies.insert(targetPath);
        }
    }

    
    for (const auto &file : std::as_const(targetOrgFiles)) {
        fixRPaths(file,
                  {"$ORIGIN/" + Utils::cleanPath(fs::relative(dest, file.parent_path())).string()});
    }

    
    for (const auto &file : std::as_const(targetDependencies)) {
        fixRPaths(file, {"$ORIGIN"});
    }

    
    
    
    
    

    
    
    
    
    
    
    
    
    
    
    
    
    
    
    

    
    

    
    

    
    
    

    
    
    
    
    
    
    

    
    
    
    
    
#  endif
#endif
    return 0;
}

int main(int argc, char *argv[]) {
    SCL::Command copyCommand = []() {
        SCL::Command command("copy", "Copy files or directories if different");
        command.addArguments({
            SCL::Argument("src", "Source files or directories").multi(),
            SCL::Argument("dest", "Destination directory"),
        });
        command.addOptions({
            SCL::Option({"-e", "--exclude"}, "Exclude a path pattern").arg("regex").multi(),
            SCL::Option({"-f", "--force"}, "Force overwrite existing files"),
        });
        command.addOptions({SCL::Option::Verbose});
        command.setHandler(cmd_copy);
        return command;
    }();

    SCL::Command rmdirCommand = []() {
        SCL::Command command("rmdir", "Remove empty directories recursively");
        command.addArguments({
            SCL::Argument("dir", "Directories").multi(),
        });
        command.addOption({SCL::Option::Verbose});
        command.setHandler(cmd_rmdir);
        return command;
    }();

    SCL::Command touchCommand = []() {
        SCL::Command command("touch", "Update file timestamp");
        command.addArguments({
            SCL::Argument("file", "File to update time stamp"),
            SCL::Argument("ref file", "Reference file", false),
        });
        command.addOption({SCL::Option::Verbose});
        command.setHandler(cmd_touch);
        return command;
    }();

    SCL::Command configureCommand = []() {
        SCL::Command command("configure", "Generate configuration header");
        command.addArgument(SCL::Argument("output file", "Output header path"));
        command.addOptions({
            SCL::Option({"-D", "--define"},
                        R"(Define a variable, format: <key>, <key>=<value>, %<raw>)")
                .arg("expr")
                .multi()
                .short_match(SCL::Option::ShortMatchSingleChar),
            SCL::Option({"-p", "--project"}, "Set project name").arg("name"),
            SCL::Option({"-w", "--warning"}, "Generate warning text").arg("file", false),
            SCL::Option({"-f", "--force"}, "Skip calculating hash and overwrite always"),
            SCL::Option({"-d", "--dryrun"}, "Print contents only"),
        });
        command.addOption({SCL::Option::Verbose});
        command.setHandler(cmd_configure);
        return command;
    }();

    SCL::Command incsyncCommand = []() {
        SCL::Command command("incsync", "Reorganize header files of include directory");
        command.addArguments({
            SCL::Argument("src", "Input directory containing source headers"),
            SCL::Argument("dest", "Output directory of reorganized headers"),
        });
        command.addOptions({
            SCL::Option({"-i", "--include"}, "Add a path pattern and corresponding subdirectory")
                .arg("regex")
                .arg("subdir")
                .multi(),
            SCL::Option({"-e", "--exclude"}, "Exclude a path pattern").arg("regex").multi(),
            SCL::Option({"-s", "--standard"}, "Add standard public-private name pattern"),
            SCL::Option({"-n", "--not-all"}, "Ignore unclassified files"),
            SCL::Option({"-c", "--copy"}, "Copy files rather than indirect reference"),
            SCL::Option({"-d", "--dryrun"}, "Print reorganizing details only"),
            SCL::Option({"-f", "--force"}, "Force deleting existing directory"),
        });
        command.addOption({SCL::Option::Verbose});
        command.setHandler(cmd_incsync);
        return command;
    }();

    SCL::Command deployCommand = []() {
        SCL::Command command("deploy", "Resolve and deploy " OS_EXECUTABLE " files' dependencies");
        command.addArguments({
            SCL::Argument("file", OS_EXECUTABLE " file(s)").multi(),
        });
        command.addOptions({
            SCL::Option({"-c", "--copy"}, "Additional " OS_EXECUTABLE " file(s) to copy")
                .arg("src")
                .arg("dir")
                .multi()
                .prior(SCL::Option::IgnoreMissingArguments),
            SCL::Option({"-o", "--out"},
                        "Set output directory of dependencies, defult to current directory")
                .arg("dir"),
#if 1
            SCL::Option({"-L", "--linkdir"}, "Add a library searching path")
                .arg("dir")
                .multi()
                .short_match(SCL::Option::ShortMatchSingleChar),
            SCL::Option({"-@", "--linkdirs"}, "Add library searching paths from a list file")
                .arg("file")
                .multi()
                .short_match(SCL::Option::ShortMatchSingleChar),
#endif
            SCL::Option({"-e", "--exclude"}, "Exclude a path pattern").arg("regex").multi(),
            SCL::Option({"-s", "--standard"}, "Ignore C/C++ runtime and system libraries"),
            SCL::Option({"-d", "--dryrun"}, "Print dependencies only"),
            SCL::Option({"-f", "--force"}, "Force overwrite existing files"),
        });
        command.addOption({SCL::Option::Verbose});
        command.setHandler(cmd_deploy);
        return command;
    }();

    SCL::Command rootCommand(SCL::appName(),
                             "Cross-platform utility commands for C/C++ build systems.");
    rootCommand.addCommands({
        copyCommand,
        rmdirCommand,
        touchCommand,
        configureCommand,
        incsyncCommand,
        deployCommand,
    });
    rootCommand.addVersionOption(TOOL_VERSION);
    rootCommand.addHelpOption(true, true);
    rootCommand.setHandler([](const SCL::ParseResult &result) -> int {
        result.showHelpText();
        return 0;
    });

    SCL::CommandCatalogue cc;
    cc.addCommands("Filesystem Commands", {"copy", "rmdir", "touch"});
    cc.addCommands("Buildsystem Commands", {"configure", "incsync", "deploy"});
    rootCommand.setCatalogue(cc);

    SCL::HelpLayout hl = []() {
        SCL::HelpLayout res;
        res.addHelpTextItem(SCL::HelpLayout::HT_Prologue);
        res.addMessageItem(SCL::HelpLayout::MI_Information);
        res.addMessageItem(SCL::HelpLayout::MI_Warning);
        res.addMessageItem(SCL::HelpLayout::MI_Critical);
        res.addHelpTextItem(SCL::HelpLayout::HT_Description);
        res.addHelpTextItem(SCL::HelpLayout::HT_Usage);
        res.addHelpListItem(SCL::HelpLayout::HL_Arguments);
        res.addHelpListItem(SCL::HelpLayout::HL_Options);
        res.addHelpListItem(SCL::HelpLayout::HL_Commands);
        res.addHelpTextItem(SCL::HelpLayout::HT_Epilogue, [](const SCL::HelpLayout::Context &ctx) {
            SCL::u8debug(SCL::MT_Warning, true, "%s\n", ctx.text->lines.c_str());
            if (ctx.hasNext) {
                SCL::u8info("\n");
            }
        });
        return res;
    }();
    rootCommand.setHelpLayout(hl);

    SCL::Parser parser(rootCommand);
    parser.setPrologue(TOOL_DESC);
    parser.setEpilogue(TOOL_COPYRIGHT);
    parser.setDisplayOptions(SCL::Parser::AlignAllCatalogues);

    int ret;
    try {
#ifdef _WIN32
        std::ignore = argc;
        std::ignore = argv;
        ret = parser.invoke(SCL::commandLineArguments());
#else
        ret = parser.invoke(argc, argv);
#endif
    } catch (const std::exception &e) {
        std::string msg = e.what();

#ifdef _WIN32
        if (typeid(e) == typeid(fs::filesystem_error)) {
            auto err = static_cast<const fs::filesystem_error &>(e);
            
            msg = SCL::ansiToUtf8(err.what());
        }
#endif

        SCL::u8debug(SCL::MT_Critical, true, "Error: %s\n", msg.data());
        ret = -1;
    }
    return ret;
}
