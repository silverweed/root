/**
  \file hadd.cxx
  \brief This program will merge compatible ROOT objects, such as histograms, Trees and RNTuples,
         from a list of root files and write them to a target root file.
         In order for a ROOT object to be mergeable, it must implement the Merge() function.
         Non-mergeable objects will have all instances copied as-is into the target file.
         The target file must not be identical to one of the source files.

  Syntax:
  ```{.cpp}
       hadd [flags] targetfile source1 source2 ... [flags]
  ```

  Flags can be passed before or after the positional arguments.
  The first positional (non-flag) argument will be interpreted as the targetfile.
  After that, the first sequence of positional arguments will be interpreted as the input files.
  If two sequences of positional arguments are separated by flags, hadd will emit an error and abort.

  By default, any argument starting with `-` is interpreted as a flag. If you want to pass filenames
  starting with `-` you need to pass them after `--`:
  ```{.cpp}
       hadd [flags] -- -file1 -file2 ...
  ```
  Note that in this case you need to pass ALL positional arguments after `--`.

  If a flag requires an argument, the argument can be specified in any of these ways:

     # All equally valid:
     -j 16
     -j16
     -j=16

   The first syntax is the preferred one since it's backward-compatible with previous versions of hadd.
   The -f flag is an exception to this rule: it only supports the `-f[0-9]` syntax.

   Note that merging multiple flags is NOT supported: `-jfa` will be interpreted as -j=fa, which is invalid!

   The flags are as follows:

  \param -a                Append to the output
  \param -cachesize <SIZE> Resize the prefetching cache used to speed up I/O operations (use 0 to disable).
  \param -d <DIR>          Carry out the partial multiprocess execution in the specified directory
  \param -dbg              Enable verbosity. If -j was specified, do not not delete partial files
                           stored inside working directory.
  \param -experimental-io-features <FEATURES> Enables the corresponding experimental feature for output trees.
                                              \see ROOT::Experimental::EIOFeatures
  \param -f            Force overwriting of output file.
  \param -f[0-9]       Set target compression algorithm `i` and level `j` passing the number `i*100 + j`, e.g. `-f505`.
                       The last digit (`j`) can be set from 0 = uncompressed to 9 = highly compressed.
                       The first digit (`i`) is 1 for ZLIB, 2 for LZMA, 4 for LZ4 and 5 for ZSTD.
                       Recommended numbers are 101 (ZLIB), 207 (LZMA), 404 (LZ4), 505 (ZSTD),
                       The default value for this flag is 101 (kDefaultZLIB).
                       See ROOT::RCompressionSetting and TFile::TFile documentation for more details.
  \param -fk           Sets the target file to contain the baskets with the same compression as the input files
                       (unless -O is specified). Compresses the meta data using the compression level specified
                       in the first input or the compression setting after fk (for example 505 when using -fk505)
  \param -ff           The compression level used is the one specified in the first input
  \param -j [N_JOBS]   Parallelise the execution in `N_JOBS` processes. If the number of processes is not specified,
                       or is 0, use the system maximum.
  \param -k            Skip corrupt or non-existent files, do not exit
  \param -L <FILE>     Read the list of objects from FILE and either only merge or skip those objects depending on
                       the value of "-Ltype". FILE must contain one object name per line, which cannot contain
                       whitespaces or '/'. You can also pass TDirectory names, which apply to the entire directory
                       content. Lines beginning with '#' are ignored. If this flag is passed, "-Ltype" MUST be
                       passed as well.
  \param -Ltype <SkipListed|OnlyListed> Sets the type of operation performed on the objects listed in FILE given with
  the
                       "-L" flag. "SkipListed" will skip all the listed objects; "OnlyListed" will only merge those
                       objects. If this flag is passed, "-L" must be passed as well.
  \param -n <N_FILES>  Open at most `N` files at once (use 0 to request to use the system maximum - which is also
                       the default). This number includes both the input reading files as well as the output file.
                       Thus, if set to 1, it will be automatically replaced to a minimum of 2. If set to a too large
  value, it will be clipped to the system maximum.
  \param -O            Re-optimize basket size when merging TTree
  \param -T            Do not merge Trees
  \param -v [LEVEL]    Explicitly set the verbosity level:
                            <= 0 = only output errors;
                            1 = only output errors and warnings;
                            2 = output minimal informative messages, errors and warnings (default);
                            >= 3 = output all messages.
  \return hadd returns a status code: 0 if OK, 1 otherwise

  For example assume 3 files f1, f2, f3 containing histograms hn and Trees Tn
   - f1 with h1 h2 h3 T1
   - f2 with h1 h4 T1 T2
   - f3 with h5
  the result of
  ```
    hadd -f x.root f1.root f2.root f3.root
  ```
  will be a file x.root with h1 h2 h3 h4 h5 T1 T2
  where
   - h1 will be the sum of the 2 histograms in f1 and f2
   - T1 will be the merge of the Trees in f1 and f2

  The files may contain sub-directories.

  If the source files contains histograms and Trees, one can skip
  the Trees with
  ```
       hadd -T targetfile source1 source2 ...
  ```

  Wildcarding and indirect files are also supported
  ```
      hadd result.root  myfil*.root
  ```
  will merge all files in myfil*.root
  ```
      hadd result.root file1.root @list.txt file2. root myfil*.root
  ```
  will merge file1.root, file2.root, all files in myfil*.root
  and all files in the indirect text file list.txt ("@" as the first
  character of the file indicates an indirect file. An indirect file
  is a text file containing a list of other files, including other
  indirect files, one line per file).

  If the sources and and target compression levels are identical (default),
  the program uses the TChain::Merge function with option "fast", ie
  the merge will be done without  unzipping or unstreaming the baskets
  (i.e. direct copy of the raw byte on disk). The "fast" mode is typically
  5 times faster than the mode unzipping and unstreaming the baskets.

  If the option -cachesize is used, hadd will resize (or disable if 0) the
  prefetching cache use to speed up I/O operations.

  For options that take a size as argument, a decimal number of bytes is expected.
  If the number ends with a `k`, `m`, `g`, etc., the number is multiplied
  by 1000 (1K), 1000000 (1MB), 1000000000 (1G), etc.
  If this prefix is followed by `i`, the number is multiplied by the traditional
  1024 (1KiB), 1048576 (1MiB), 1073741824 (1GiB), etc.
  The prefix can be optionally followed by B whose casing is ignored,
  eg. 1k, 1K, 1Kb and 1KB are the same.

  \note By default histograms are added. However hadd does not support the case where
         histograms have their bit TH1::kIsAverage set.

  \authors Rene Brun, Dirk Geppert, Sven A. Schmidt, Toby Burnett
*/
#include "Compression.h"
#include "TClass.h"
#include "TFile.h"
#include "TFileMerger.h"
#include "THashList.h"
#include "TKey.h"
#include "TSystem.h"
#include "TUUID.h"

#include <ROOT/RConfig.hxx>
#include <ROOT/StringConv.hxx>
#include <ROOT/TIOFeatures.hxx>

#include "haddCommandLineOptionsHelp.h"
#include "logging.hxx"

#include "optparse.hxx"

#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <streambuf>
#include <variant>

#ifndef R__WIN32
#include "ROOT/TProcessExecutor.hxx"
#endif

////////////////////////////////////////////////////////////////////////////////

// NOTE: TFileMerger will use PrintLevel = GetLogVerbosity() - 1. If PrintLevel is < 1, it will print nothing, otherwise
// it will print everything. To give some granularity to hadd, we do the following:
// LogVerbosity = 0: only print hadd errors
// LogVerbosity = 1: only print hadd errors + warnings
// LogVerbosity = 2: print hadd errors + warnings and TFileMerger messages
// LogVerbosity > 2: print all hadd and TFileMerger messages.
static constexpr int kDefaultVerbosity = 2;

using IntFlag_t = uint32_t;
struct CompressionKeepFirstInput {};

enum class EObjectFilterType {
   kUnspecified,
   kOnlyListed = TFileMerger::kOnlyListed,
   kSkipListed = TFileMerger::kSkipListed,
};

enum class EHelpType {
   kNone,
   kShort,
   kLong,
};

struct HAddArgs {
   bool fAppend = false;
   bool fForce = false;
   bool fSkipErrors = false;
   std::string fHelp;

   struct TTreeOpts {
      bool fNoTrees = false;
      bool fReoptimize = false;
      bool fKeepCompressionAsIs = false;
      ROOT::TIOFeatures fFeatures;
   } fTTree;

   std::string fWorkingDir;
   std::optional<IntFlag_t> fNProcesses;
   std::string fObjectFilterFile;
   EObjectFilterType fObjectFilterType = EObjectFilterType::kUnspecified;
   TString fCacheSize;
   std::optional<IntFlag_t> fMaxOpenedFiles;
   std::optional<IntFlag_t> fVerbosity;
   std::variant<std::monostate, IntFlag_t, CompressionKeepFirstInput> fCompressionSettings;

   std::string fTarget;
   std::vector<std::string> fSources;
};

enum class EFlagResult {
   kIgnored,
   kParsed,
   kErr
};

template <typename T>
struct FlagConvResult {
   T fValue;
   EFlagResult fResult;
};

EFlagResult ConvertIOFeatures(std::string_view arg, ROOT::TIOFeatures &features)
{
   std::stringstream ss;
   ss << arg;
   std::string item;
   while (std::getline(ss, item, ',')) {
      if (!features.Set(item))
         Warn() << "ignoring unknown feature request: " << item << "\n";
   }
   return EFlagResult::kParsed;
}

static FlagConvResult<TString> ConvertCacheSize(std::string_view arg)
{
   TString cacheSize;
   int size;
   auto parseResult = ROOT::FromHumanReadableSize(arg, size);
   if (parseResult == ROOT::EFromHumanReadableSize::kParseFail) {
      Err() << "could not parse the cache size passed after --cachesize: '" << arg << "'\n";
      return {"", EFlagResult::kErr};
   } else if (parseResult == ROOT::EFromHumanReadableSize::kOverflow) {
      double m;
      const char *munit = nullptr;
      ROOT::ToHumanReadableSize(INT_MAX, false, &m, &munit);
      Warn() << "the cache size passed after -cachesize is too large: " << arg << " is greater than " << m << munit
             << ". We will use the maximum value.\n";
      return {std::to_string(m) + munit, EFlagResult::kParsed};
   } else {
      cacheSize = "cachesize=";
      cacheSize.Append(std::string(arg));
   }
   return {cacheSize, EFlagResult::kParsed};
}

static FlagConvResult<Int_t> ConvertFilterType(const char *arg)
{
   if (strcmp(arg, "SkipListed") == 0)
      return {TFileMerger::kSkipListed, EFlagResult::kParsed};
   if (strcmp(arg, "OnlyListed") == 0)
      return {TFileMerger::kOnlyListed, EFlagResult::kParsed};

   Err() << "invalid argument for -Ltype: '" << arg << "'. Can only be 'SkipListed' or 'OnlyListed' (case matters).\n";
   return {{}, EFlagResult::kErr};
}

static bool ValidCompressionSettings(int compSettings)
{
   // Must be a number between 0 and 509 (with a 0 in the middle)
   if (compSettings == 0)
      return true;
   // We also accept [1-9] as aliases of [101-109], but it's discouraged.
   if (compSettings >= 1 && compSettings <= 9) {
      Warn() << "interpreting " << compSettings << " as " << 100 + compSettings
             << "."
                " This behavior is deprecated, please use the full compression settings.\n";
      return true;
   }
   return (compSettings >= 100 && compSettings <= 509) && ((compSettings / 10) % 10 == 0);
}

// Returns the flags to add to the file merger's flags, or -1 in case of errors.
static Int_t ParseFilterFile(const std::optional<std::string> &filterFileName, std::optional<Int_t> objectFilterType,
                             TFileMerger &fileMerger)
{
   if (filterFileName) {
      std::ifstream filterFile(*filterFileName);
      if (!filterFile) {
         Err() << "error opening filter file '" << *filterFileName << "'\n";
         return -1;
      }
      TString filteredObjects;
      std::string line;
      std::string objPath;
      int nObjects = 0;
      while (std::getline(filterFile, line)) {
         std::istringstream ss(line);
         // only read exactly 1 token per line (strips any whitespaces and such)
         objPath.clear();
         ss >> objPath;
         if (!objPath.empty() && objPath[0] != '#') {
            filteredObjects.Append(objPath + ' ');
            ++nObjects;
         }
      }

      if (nObjects) {
         Info(2) << "added " << nObjects << " object from filter file '" << *filterFileName << "'\n";
         fileMerger.AddObjectNames(filteredObjects);
      } else {
         Warn() << "no objects were added from filter file '" << *filterFileName << "'\n";
      }

      assert(objectFilterType.has_value());
      const auto filterFlag = *objectFilterType;
      assert(filterFlag == TFileMerger::kSkipListed || filterFlag == TFileMerger::kOnlyListed);
      return filterFlag;
   }
   return 0;
}

static bool FilesAreEquivalent(std::string_view source, std::string_view target)
{
   const bool sourceHasProtocol = source.find_first_of("://") == std::string_view::npos;
   const bool targetHasProtocol = target.find_first_of("://") == std::string_view::npos;
   if (sourceHasProtocol != targetHasProtocol)
      return false;

   // We cannot use std::filesystem functions for file paths that have a protocol.
   if (!sourceHasProtocol)
      return source == target;

   return std::filesystem::exists(target) && std::filesystem::equivalent(source, target);
}

static EObjectFilterType ConvertObjectFilterType(std::string_view filtType)
{
   if (filtType.empty())
      return EObjectFilterType::kUnspecified;
   if (filtType == "OnlyListed")
      return EObjectFilterType::kOnlyListed;
   if (filtType == "SkipListed")
      return EObjectFilterType::kSkipListed;

   throw std::invalid_argument(std::string("invalid value for --list-type: ") + std::string(filtType));
}

static HAddArgs ParseArgs(const char **args, int nArgs)
{
   using FT = ROOT::RCmdLineOpts::EFlagType;

   ROOT::RCmdLineOpts opts;
   opts.AddFlag({"-a", "--append"}, FT::kSwitch, "Append to the output");
   opts.AddFlag({"-c", "--compression"}, FT::kWithArg,
                "Set target compression. The argument must be a number of the format `i0j`, where `i` represents the "
                "compression algorithm and `j` the compression level used."
                " 0 means 'uncompressed'. For the compression algorithm: 1=Zlib, 2=LZMA, 4=LZ4, 5=ZSTD. For the "
                "compression level, any number from 0 to 9 is accepted. See ROOT::RCompressionSetting and TFile::TFile "
                "documentation for more details. If \"same\" is passed instead of a number, use the same compression "
                "as the first input file.");
   opts.AddFlag({"--cachesize"}, FT::kWithArg,
                "Resize the prefetching cache used to speed up I/O operations (0 to disable)");
   opts.AddFlag({"--workdir"}, FT::kWithArg, "Carry out the partial multiprocess execution in the specified directory");
   opts.AddFlag({"-f", "--force-overwrite"}, FT::kSwitch, "Force overwriting of output file");
   opts.AddFlag({"-j", "--jobs"}, FT::kWithArg,
                "Parallelise the execution in N processes. Pass 0 to use the system maximum");
   opts.AddFlag({"-k", "--skip-errors"}, FT::kSwitch, "Skip corrupt or non-existent files, do not exit");
   opts.AddFlag({"-L", "--from-list"}, FT::kWithArg,
                "Read the list of objects from FILE and either only merge or skip those objects depending on"
                " the value of \"--list-type\". FILE must contain one object name per line, which cannot contain"
                " whitespaces or '/'. You can also pass TDirectory names, which apply to the entire directory"
                " content. Lines beginning with '#' are ignored. If this flag is passed, \"--list-type\" MUST be"
                " passed as well.");
   opts.AddFlag({"--Ltype", "--list-type"}, FT::kWithArg,
                "Sets the type of operation performed on the objects listed in the file given with the \"-L\" flag."
                "\"SkipListed\" will skip all the listed objects; \"OnlyListed\" will only merge those"
                " objects. If this flag is passed, \"-L\" must be passed as well.");
   opts.AddFlag({"-n", "--max-files"}, FT::kWithArg,
                "Open at most N files at once (pass 0 to use the system maximum - which is also the default)."
                " This number includes both the input reading files as well as the output file."
                " Thus, if set to 1, it will be automatically replaced to a minimum of 2. If set to too large of a "
                " value, it will be capped to the system maximum.");
   opts.AddFlag({"-v"}, FT::kSwitch, "Set verbosity level. Pass multiple times to increase verbosity.",
                ROOT::RCmdLineOpts::kFlagAllowMultiple);
   // TTree-specific flags
   opts.AddFlag({"--ttree-experimental-io-features"}, FT::kWithArg,
                "Enables the corresponding experimental feature for output trees",
                ROOT::RCmdLineOpts::kFlagAllowMultiple);
   opts.AddFlag({"--ttree-reoptimize-basket-size"}, FT::kSwitch, "Re-optimize basket size when merging TTrees");
   opts.AddFlag({"--ttree-skip"}, FT::kSwitch, "Do not merge TTrees");
   // TODO: make shared with RNTuple
   opts.AddFlag({"--ttree-keep-basket-compression"}, FT::kSwitch,
                "Sets the target file to contain TTree baskets with the same compression as the input files"
                " (unless --ttree-reoptimize-basket-size is specified). Compresses the meta data using the compression "
                "level specified"
                " in the first input or the compression setting given with -c (for example 505 when using -c 505 "
                "--ttree-keep-basket-compression)");
   opts.AddFlag({"-h", "--help"}, FT::kSwitch, "Get a help message");

   opts.Parse(args, nArgs);

   assert(opts.GetSettings().fFlagTreatment == ROOT::RCmdLineOpts::EFlagTreatment::kGrouped);

   HAddArgs parsedArgs;
   auto doHelp = EHelpType::kNone;
   try {
      parsedArgs.fAppend = opts.GetSwitch("append");
      if (const auto cacheSize = opts.GetFlagValue("cachesize"); !cacheSize.empty()) {
         if (const auto parsedCacheSize = ConvertCacheSize(cacheSize); parsedCacheSize.fResult == EFlagResult::kParsed)
            parsedArgs.fCacheSize = parsedCacheSize.fValue;
      }
      parsedArgs.fWorkingDir = opts.GetFlagValue("workdir");
      for (const auto ioFeatures : opts.GetFlagValues("ttree-experimental-io-features")) {
         if (ConvertIOFeatures(ioFeatures, parsedArgs.fTTree.fFeatures) != EFlagResult::kParsed)
            doHelp = EHelpType::kShort;
      }
      parsedArgs.fForce = opts.GetSwitch("force-overwrite");
      if (const auto compSettings = opts.GetFlagValue("compression"); compSettings == "same") {
         parsedArgs.fCompressionSettings = CompressionKeepFirstInput{};
      } else {
         if (const auto compSettingsInt = opts.GetFlagValueAs<IntFlag_t>("compression"))
            parsedArgs.fCompressionSettings = *compSettingsInt;
      }
      parsedArgs.fTTree.fKeepCompressionAsIs = opts.GetSwitch("ttree-keep-basket-compression");
      parsedArgs.fNProcesses = opts.GetFlagValueAs<IntFlag_t>("jobs");
      parsedArgs.fSkipErrors = opts.GetSwitch("skip-errors");
      parsedArgs.fObjectFilterFile = opts.GetFlagValue("from-list");
      parsedArgs.fObjectFilterType = ConvertObjectFilterType(opts.GetFlagValue("list-type"));
      parsedArgs.fTTree.fNoTrees = opts.GetSwitch("ttree-skip");
      parsedArgs.fMaxOpenedFiles = opts.GetFlagValueAs<IntFlag_t>("max-files");
      parsedArgs.fTTree.fReoptimize = opts.GetSwitch("ttree-reoptimize-basket-size");
      parsedArgs.fTTree.fNoTrees = opts.GetSwitch("ttree-skip");
      parsedArgs.fVerbosity = opts.GetSwitch("v");
      if (parsedArgs.fVerbosity == 0)
         parsedArgs.fVerbosity = kDefaultVerbosity;

      parsedArgs.fSources = opts.GetArgs();
      if (parsedArgs.fSources.size() < 2) {
         doHelp = EHelpType::kShort;
      } else {
         parsedArgs.fTarget = parsedArgs.fSources.front();
         parsedArgs.fSources.erase(parsedArgs.fSources.begin());
      }
   } catch (const std::runtime_error &ex) {
      Err() << ex.what() << "\n";
      doHelp = EHelpType::kShort;
   }

   // Done at the end so it doesn't get overridden.
   if (opts.GetSwitch("help"))
      doHelp = EHelpType::kLong;

   if (doHelp == EHelpType::kShort)
      parsedArgs.fHelp = opts.MakeShortHelp(Detail::gLogName);
   else if (doHelp == EHelpType::kLong)
      parsedArgs.fHelp = opts.MakeLongHelp(Detail::gLogName, "");

   return parsedArgs;
}

int main(int argc, const char **argv)
{
   InitLog("rootmerge", kDefaultVerbosity);

   const auto args = ParseArgs(argv + 1, argc - 1);
   if (!args.fHelp.empty()) {
      std::cerr << args.fHelp << "\n";
      return 0;
   }
#if 0
   ROOT::TIOFeatures features = args.fFeatures.value_or(ROOT::TIOFeatures{});
   Int_t maxopenedfiles = args.fMaxOpenedFiles.value_or(0);
   SetLogVerbosity(args.fVerbosity.value_or(kDefaultHaddVerbosity));
   Int_t newcomp = args.fCompressionSettings.value_or(-1);
   TString cacheSize = args.fCacheSize.value_or("");

   // For the -j flag (nProcesses), we check if the flag is present and, if so, if it has a
   // valid value (i.e. any value > 0).
   // If the flag is present at all, we do multiprocessing. If the value of nProcesses is invalid,
   // we default to the number of cpus on the machine.
   Bool_t multiproc = args.fNProcesses.has_value();
   int nProcesses;
   if (args.fNProcesses && *args.fNProcesses > 0) {
      nProcesses = *args.fNProcesses;
   } else {
      SysInfo_t s;
      gSystem->GetSysInfo(&s);
      nProcesses = s.fCpus;
   }
   if (multiproc)
      Info(2) << "parallelizing  with " << nProcesses << " processes.\n";

   // If the user specified a workingDir, use that. Otherwise, default to the system temp dir.
   std::string workingDir;
   if (!args.fWorkingDir) {
      workingDir = gSystem->TempDirectory();
   } else if (args.fWorkingDir && gSystem->AccessPathName(args.fWorkingDir->c_str())) {
      Err() << "could not access the directory specified: " << *args.fWorkingDir << ".\n";
      return 1;
   } else {
      workingDir = *args.fWorkingDir;
   }

   // Verify that -L and -Ltype are either both present or both absent.
   if (args.fObjectFilterFile.has_value() != args.fObjectFilterType.has_value()) {
      Err() << "-L must always be passed along with -Ltype.\n";
      return 1;
   }

   const char *targetname = 0;
   if (!args.fOutputArgIdx) {
      Err() << "missing output file.\n";
      fputs(kCommandLineShortHelp, stderr);
      return 1;
   }
   if (!args.fFirstInputIdx) {
      Err() << "missing input file.\n";
      fputs(kCommandLineShortHelp, stderr);
      return 1;
   }
   targetname = argv[args.fOutputArgIdx];

   Info(2) << "target file: " << targetname << "\n";

   if (args.fCacheSize)
      Info(2) << "Using " << cacheSize << "\n";

   ////////////////////////////// end flags processing /////////////////////////////////

   gSystem->Load("libTreePlayer");

   TFileMerger fileMerger(kFALSE, kFALSE);
   fileMerger.SetMsgPrefix("hadd");
   fileMerger.SetPrintLevel(GetLogVerbosity() - 1);
   if (maxopenedfiles > 0) {
      fileMerger.SetMaxOpenedFiles(maxopenedfiles);
   }
   // The following section will collect all input filenames into a vector,
   // including those listed within an indirect file.
   // If any file can not be accessed, it will error out, unless args.fSkipErrors is true
   std::vector<std::string> allSubfiles;
   for (int a = args.fFirstInputIdx; a < argc; ++a) {
      if (!args.fNoFlagsAfterPositionalArguments && argv[a] && argv[a][0] == '-') {
         break;
      }
      if (argv[a] && argv[a][0] == '@') {
         std::ifstream indirect_file(argv[a] + 1);
         if (!indirect_file.is_open()) {
            Err() << "could not open indirect file " << (argv[a] + 1) << std::endl;
            if (!args.fSkipErrors)
               return 1;
         } else {
            std::string line;
            while (indirect_file) {
               if (std::getline(indirect_file, line) && line.length()) {
                  if (gSystem->AccessPathName(line.c_str(), kReadPermission) == kTRUE) {
                     Err() << "could not validate the file name \"" << line << "\" within indirect file "
                           << (argv[a] + 1) << std::endl;
                     if (!args.fSkipErrors)
                        return 1;
                  } else if (FilesAreEquivalent(line, targetname)) {
                     Err() << "file " << line << " cannot be both the target and an input!\n";
                     if (!args.fSkipErrors)
                        return 1;
                  } else {
                     allSubfiles.emplace_back(line);
                  }
               }
            }
         }
      } else {
         const char *line = argv[a];
         if (gSystem->AccessPathName(line, kReadPermission) == kTRUE) {
            Err() << "could not validate argument \"" << line << "\" as input file " << std::endl;
            if (!args.fSkipErrors)
               return 1;
         } else if (FilesAreEquivalent(line, targetname)) {
            Err() << "file " << line << " cannot be both the target and an input!\n";
            if (!args.fSkipErrors)
               return 1;
         } else {
            allSubfiles.emplace_back(line);
         }
      }
   }
   if (allSubfiles.empty()) {
      Err() << "could not find any valid input file " << std::endl;
      return 1;
   }
   // The next snippet determines the output compression if unset
   if (newcomp == -1) {
      if (args.fUseFirstInputCompression || args.fKeepCompressionAsIs) {
         // grab from the first file.
         TFile *firstInput = TFile::Open(allSubfiles.front().c_str());
         if (firstInput && !firstInput->IsZombie())
            newcomp = firstInput->GetCompressionSettings();
         else
            newcomp = ROOT::RCompressionSetting::EDefaults::kUseCompiledDefault;
         delete firstInput;
         fileMerger.SetMergeOptions(TString("FirstSrcCompression"));
      } else {
         newcomp = ROOT::RCompressionSetting::EDefaults::kUseCompiledDefault;
         fileMerger.SetMergeOptions(TString("DefaultCompression"));
      }
   }
   if (args.fKeepCompressionAsIs && !args.fReoptimize)
      Info(2) << "compression setting for meta data: " << newcomp << '\n';
   else
      Info(2) << "compression setting for all output: " << newcomp << '\n';

   if (args.fAppend) {
      if (!fileMerger.OutputFile(targetname, "UPDATE", newcomp)) {
         Err() << "error opening target file for update :" << targetname << ".\n";
         return 2;
      }
   } else if (!fileMerger.OutputFile(targetname, args.fForce, newcomp)) {
      std::stringstream ss;
      ss << "error opening target file (does " << targetname << " exist?).\n";
      if (!args.fForce)
         ss << "pass \"-f\" argument to force re-creation of output file.\n";
      Err() << ss.str();
      return 1;
   }

   auto step = (allSubfiles.size() + nProcesses - 1) / nProcesses;
   if (multiproc && step < 3) {
      // At least 3 files per process
      step = 3;
      nProcesses = (allSubfiles.size() + step - 1) / step;
      Info(2) << "each process should handle at least 3 files for efficiency."
                 " Setting the number of processes to: "
              << nProcesses << std::endl;
   }
   if (nProcesses == 1)
      multiproc = kFALSE;

   std::vector<std::string> partialFiles;

#ifndef R__WIN32
   // this is commented out only to try to prevent false positive detection
   // from several anti-virus engines on Windows, and multiproc is not
   // supported on Windows anyway
   if (multiproc) {
      auto uuid = TUUID();
      auto partialTail = uuid.AsString();
      for (auto i = 0; (i * step) < allSubfiles.size(); i++) {
         std::stringstream buffer;
         buffer << workingDir << "/partial" << i << "_" << partialTail << ".root";
         partialFiles.emplace_back(buffer.str());
      }
   }
#endif

   auto mergeFiles = [&](TFileMerger &merger) {
      if (args.fReoptimize) {
         merger.SetFastMethod(kFALSE);
      } else {
         if (!args.fKeepCompressionAsIs && merger.HasCompressionChange()) {
            // Don't warn if the user has requested any re-optimization.
            Warn() << "Sources and Target have different compression settings\n"
                      "hadd merging will be slower\n";
         }
      }
      merger.SetNotrees(args.fNoTrees);
      merger.SetMergeOptions(TString(merger.GetMergeOptions()) + " " + cacheSize);
      merger.SetErrorBehavior(args.fSkipErrors ? TFileMerger::EErrorBehavior::kSkipOnError
                                               : TFileMerger::EErrorBehavior::kFailOnError);
      merger.SetIOFeatures(features);
      Int_t fileMergerFlags = TFileMerger::kAll;
      Int_t extraFlags = ParseFilterFile(args.fObjectFilterFile, args.fObjectFilterType, merger);
      if (extraFlags < 0)
         return false;
      fileMergerFlags |= extraFlags;
      if (args.fAppend)
         fileMergerFlags |= TFileMerger::kIncremental;
      else
         fileMergerFlags |= TFileMerger::kRegular;
      Bool_t status = merger.PartialMerge(fileMergerFlags);
      return status;
   };

   auto sequentialMerge = [&](TFileMerger &merger, int start, int nFiles) {
      for (auto i = start; i < (start + nFiles) && i < static_cast<int>(allSubfiles.size()); i++) {
         if (!merger.AddFile(allSubfiles[i].c_str())) {
            if (args.fSkipErrors) {
               Warn() << "skipping file with error: " << allSubfiles[i] << std::endl;
            } else {
               Err() << "exiting due to error in " << allSubfiles[i] << std::endl;
               return kFALSE;
            }
         }
      }
      return mergeFiles(merger);
   };

   auto parallelMerge = [&](int start) {
      TFileMerger mergerP(kFALSE, kFALSE);
      mergerP.SetMsgPrefix("hadd");
      mergerP.SetPrintLevel(GetLogVerbosity() - 1);
      if (maxopenedfiles > 0) {
         mergerP.SetMaxOpenedFiles(maxopenedfiles / nProcesses);
      }
      if (!mergerP.OutputFile(partialFiles[start / step].c_str(), args.fForce, newcomp)) {
         Err() << "error opening target partial file\n";
         exit(1);
      }
      return sequentialMerge(mergerP, start, step);
   };

   auto reductionFunc = [&]() {
      for (const auto &pf : partialFiles) {
         fileMerger.AddFile(pf.c_str());
      }
      return mergeFiles(fileMerger);
   };

   Bool_t status;

#ifndef R__WIN32
   if (multiproc) {
      ROOT::TProcessExecutor p(nProcesses);
      auto res = p.Map(parallelMerge, ROOT::TSeqI(0, allSubfiles.size(), step));
      status = std::accumulate(res.begin(), res.end(), 0U) == partialFiles.size();
      if (status) {
         status = reductionFunc();
      } else {
         Err() << "failed at the parallel stage\n";
      }
      if (!args.fDebug) {
         for (const auto &pf : partialFiles) {
            gSystem->Unlink(pf.c_str());
         }
      }
   } else {
      status = sequentialMerge(fileMerger, 0, allSubfiles.size());
   }
#else
   status = sequentialMerge(fileMerger, 0, allSubfiles.size());
#endif

   if (status) {
      Info(3) << "merged " << allSubfiles.size() << " (" << fileMerger.GetMergeList()->GetEntries()
              << ") input (partial) files into " << targetname << "\n";
      return 0;
   } else {
      Err() << "failure during the merge of " << allSubfiles.size() << " (" << fileMerger.GetMergeList()->GetEntries()
            << ") input (partial) files into " << targetname << "\n";
      return 1;
   }
#endif
}
