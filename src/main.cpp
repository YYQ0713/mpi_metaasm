#include <iostream>
#include <string>
#include <cstdlib>   // For getenv
#include <cstring>   // For strerror
#include <limits.h>  // For PATH_MAX
#include <unistd.h>  // For realpath
#include <sys/stat.h>  // For mkdir
#include <cstdio>      // For remove
#include <fstream>
#include <vector>
#include <utility> // For std::pair
#include <stdexcept>
#include <sstream>
#include <algorithm> // For std::find_if


#include "cxxopts.hpp"
#include "common/common.hpp"
#include "kmercount/kmercount.hpp"


// Helper function: Split a string by delimiter
std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream tokenStream(str);
    std::string token;
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Helper function: Join vector of strings with '/'
std::string join(const std::vector<std::string>& parts, const std::string& delimiter) {
    std::string result;
    for (const auto& part : parts) {
        if (!result.empty()) result += delimiter;
        result += part;
    }
    return result;
}

// Normalize a path by resolving ".." and "."
std::string normalize_path(const std::string& path) {
    std::vector<std::string> parts = split(path, '/');
    std::vector<std::string> normalized;

    for (const auto& part : parts) {
        if (part == "." || part.empty()) {
            continue; // Skip current directory markers and redundant slashes
        }
        if (part == "..") {
            if (!normalized.empty()) {
                normalized.pop_back(); // Go up one directory
            }
        } else {
            normalized.push_back(part); // Add normal directory/file name
        }
    }

    return "/" + join(normalized, "/");
}

std::string abspath(const std::string& path) {
    try {
        // Step 1: Expand ~ to the home directory
        std::string expanded_path = path;
        if (!path.empty() && path[0] == '~') {
            const char* home = getenv("HOME");
            if (home) {
                expanded_path.replace(0, 1, home);
            } else {
                throw std::runtime_error("Unable to expand '~': HOME environment variable not set.");
            }
        }

        // Step 2: Handle absolute paths
        if (!expanded_path.empty() && expanded_path[0] == '/') {
            return normalize_path(expanded_path); // Already an absolute path, normalize it
        }

        // Step 3: Handle relative paths by resolving against the current working directory
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            throw std::runtime_error("Failed to get current working directory: " + std::string(strerror(errno)));
        }

        std::string absolute_path = std::string(cwd) + "/" + expanded_path;

        // Step 4: Normalize the path (resolve ".." and ".")
        return normalize_path(absolute_path);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return {};
    }
}

// Helper function to determine inpipe command
std::string inpipe_cmd(const std::string& file_name) {
    if (file_name.size() >= 3 && file_name.substr(file_name.size() - 3) == ".gz") {
        return "gzip -cd " + file_name;
    } else if (file_name.size() >= 4 && file_name.substr(file_name.size() - 4) == ".bz2") {
        return "bzip2 -cd " + file_name;
    } else {
        return "";
    }
}

// Helper function to join paths (manual implementation for C++14)
std::string join_paths(const std::string& base, const std::string& sub_path) {
    if (base.empty()) return sub_path;
    if (sub_path.empty()) return base;
    if (base.back() == '/') {
        return base + sub_path;
    } else {
        return base + "/" + sub_path;
    }
}

// Function to create library file
void create_library_file(const GlobalOptions& opt) {
    try {
        // Open the library file for writing
        std::ofstream lib(opt.read_lib_path());
        if (!lib.is_open()) {
            throw std::runtime_error("Failed to open library file: " + opt.read_lib_path());
        }

        // Write interleaved paired-end files (pe12)
        for (size_t i = 0; i < opt.pe12.size(); ++i) {
            const std::string& file_name = opt.pe12[i];
            lib << file_name << "\n";
            if (!inpipe_cmd(file_name).empty()) {
                lib << "interleaved " << join_paths(opt.temp_dir, "inpipe.pe12." + std::to_string(i)) << "\n";
            } else {
                lib << "interleaved " << file_name << "\n";
            }
        }

        // Write paired-end files (pe1 and pe2)
        for (size_t i = 0; i < opt.pe1.size(); ++i) {
            const std::string& file_name1 = opt.pe1[i];
            const std::string& file_name2 = opt.pe2[i];
            lib << file_name1 << "," << file_name2 << "\n";

            std::string f1 = !inpipe_cmd(file_name1).empty()
                                 ? join_paths(opt.temp_dir, "inpipe.pe1." + std::to_string(i))
                                 : file_name1;
            std::string f2 = !inpipe_cmd(file_name2).empty()
                                 ? join_paths(opt.temp_dir, "inpipe.pe2." + std::to_string(i))
                                 : file_name2;

            lib << "pe " << f1 << " " << f2 << "\n";
        }

        // Write single-end files (se)
        for (size_t i = 0; i < opt.se.size(); ++i) {
            const std::string& file_name = opt.se[i];
            lib << file_name << "\n";
            if (!inpipe_cmd(file_name).empty()) {
                lib << "se " << join_paths(opt.temp_dir, "inpipe.se." + std::to_string(i)) << "\n";
            } else {
                lib << "se " << file_name << "\n";
            }
        }

        std::cout << "Library file created successfully: " << opt.read_lib_path() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error creating library file: " << e.what() << std::endl;
        throw;
    }
}

// Function to create a directory if it does not exist
void mkdir_if_not_exists(const std::string& path) {
    struct stat info;
    // Check if the path exists and is a directory
    if (stat(path.c_str(), &info) != 0) {
        // Path does not exist, create it
        if (mkdir(path.c_str(), 0755) != 0) {
            perror(("Failed to create directory: " + path).c_str());
        } else {
            std::cout << "Directory created: " << path << std::endl;
        }
    } else if (!(info.st_mode & S_IFDIR)) {
        std::cerr << "Error: Path exists but is not a directory: " << path << std::endl;
    }
}

// Function to remove a file if it exists
void remove_if_exists(const std::string& file_name) {
    // Check if the file exists
    if (access(file_name.c_str(), F_OK) == 0) {
        // File exists, remove it
        if (remove(file_name.c_str()) != 0) {
            perror(("Failed to remove file: " + file_name).c_str());
        } else {
            std::cout << "File removed: " << file_name << std::endl;
        }
    } else {
        std::cout << "File does not exist: " << file_name << std::endl;
    }
}

// Temporary directory creation
std::string mkdtemp_wrapper(const std::string& dir, const std::string& prefix) {
    std::string template_path = dir + "/" + prefix + "XXXXXX";
    char temp_path[PATH_MAX];
    strncpy(temp_path, template_path.c_str(), PATH_MAX);
    if (mkdtemp(temp_path) == nullptr) {
        perror("Failed to create temporary directory");
        throw std::runtime_error("Failed to create temporary directory");
    }
    return std::string(temp_path);
}

// Function to set up output directory
void setup_output_dir(GlobalOptions& opt) {
    try {
        // Set default output directory
        if (opt.out_dir.empty()) {
            opt.out_dir = abspath("./metagasm_out");
        }

        // Check if output directory already exists
        if (!opt.force_overwrite && !opt.test_mode) {
            struct stat info;
            if (stat(opt.out_dir.c_str(), &info) == 0) {
                throw std::runtime_error(
                    "Output directory " + opt.out_dir +
                    " already exists. Please change the parameter to avoid overwriting.");
            }
        }

        // Set up temp_dir
        if (opt.temp_dir.empty()) {
            opt.temp_dir = opt.out_dir + "/tmp";
        } else {
            opt.temp_dir = mkdtemp_wrapper(opt.temp_dir, "megahit_tmp_");
        }

        // Create directories
        if (opt.mpipars.rank == 0)
        {
            mkdir_if_not_exists(opt.out_dir);
            mkdir_if_not_exists(opt.temp_dir);
            mkdir_if_not_exists(opt.contig_dir());

            std::cout << "Directories successfully set up:\n";
            std::cout << "Output directory: " << opt.out_dir << '\n';
            std::cout << "Temporary directory: " << opt.temp_dir << '\n';
            std::cout << "Contig directory: " << opt.contig_dir() << '\n';
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error setting up directories: " << e.what() << '\n';
        throw;
    }
}

std::string graph_prefix(GlobalOptions& opt) {
    std::string dir = opt.temp_dir + "/k" + std::to_string(opt.k);
    
    // Create directory if it doesn't exist
    if (opt.mpipars.rank == 0)
    {
        mkdir_if_not_exists(dir);
    }
    
    return dir + "/" + std::to_string(opt.k);
}

int main_build_lib(GlobalOptions& opts);
int main_kmer_count(GlobalOptions& opts);

int main(int argc, char **argv) {

    MPIEnviroment mpipars;
    mpipars.init(argc, argv);

    cxxopts::Options options("metagasm", "metagenome asm");
    

	options.add_options()
    ("1, pe1", "comma-separated list of fasta/q paired-end #1 files, paired with files in <pe2>", cxxopts::value<std::vector<std::string>>())
    ("2, pe2", "comma-separated list of fasta/q paired-end #2 files, paired with files in <pe1>", cxxopts::value<std::vector<std::string>>())
    ("p, pe12", "comma-separated list of interleaved fasta/q paired-end files", cxxopts::value<std::vector<std::string>>())
    ("r, se", "comma-separated list of fasta/q single-end files", cxxopts::value<std::vector<std::string>>())
    ("o, out-dir", "output directory [./megahit_out]", cxxopts::value<std::string>())
    ("tmp-dir", "set temp directory", cxxopts::value<std::string>())
    ("k-min", "minimum kmer size, must be odd number [21]", cxxopts::value<int>())
    ("k-max", "maximum kmer size, must be odd number [141]", cxxopts::value<int>())
    ("k-step", "increment of kmer size of each iteration (<= 28), must be even number [10]", cxxopts::value<int>())
    ("t, num-cpu-threads", "number of CPU threads", cxxopts::value<int>())
    ("mem-flag", "SdBG builder memory mode. 0: minimum; 1: moderate; others: use all memory specified by '-m/--memory' [1]", cxxopts::value<int>())
    ("m, memory", "max memory in byte to be used in SdBG construction (if set between 0-1, fraction of the machine's total memory) [0.9]", cxxopts::value<float>())
    ("min-count", "minimum multiplicity for filtering (k_min+1)-mers [2]", cxxopts::value<int>())
    ("out-prefix", "output prefix (the contig file will be OUT_DIR/OUT_PREFIX.contigs.fa)", cxxopts::value<std::string>())
    ("h, help", "Usage")
    ;

	auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
      std::cout << options.help() << std::endl;
      exit(0);
    }

    GlobalOptions mopts;

    mopts.mpipars = mpipars;

    if (result.count("1"))
    {
        mopts.pe1 = result["1"].as<std::vector<std::string>>();
        for (size_t i = 0; i < mopts.pe1.size(); i++)
        {
            mopts.pe1[i] = abspath(mopts.pe1[i]);
        }
    }

    if (result.count("2"))
    {
        mopts.pe2 = result["2"].as<std::vector<std::string>>();
        for (size_t i = 0; i < mopts.pe2.size(); i++)
        {
            mopts.pe2[i] = abspath(mopts.pe2[i]);
        }
    }

    if (result.count("p"))
    {
        mopts.pe12 = result["p"].as<std::vector<std::string>>();
        for (size_t i = 0; i < mopts.pe12.size(); i++)
        {
            mopts.pe12[i] = abspath(mopts.pe12[i]);
        }
    }

    if (result.count("r"))
    {
        mopts.se = result["r"].as<std::vector<std::string>>();
        for (size_t i = 0; i < mopts.se.size(); i++)
        {
            mopts.se[i] = abspath(mopts.se[i]);
        }
    }

    if (result.count("t"))
    {
        mopts.num_cpu_threads = result["t"].as<int>();
        std::cout << mopts.num_cpu_threads << std::endl;
    }
    std::cout << mopts.host_mem() << std::endl;
    
    setup_output_dir(mopts);
    
    if (mpipars.rank == 0)
    {
        create_library_file(mopts);
        main_build_lib(mopts);
    }

    MPI_Barrier(MPI_COMM_WORLD);


    if (result.count("k-min"))
    {
        mopts.k = result["k-min"].as<int>();
    }

    if (result.count("min-count"))
    {
        mopts.min_count = result["min-count"].as<int>();
    }

    if (result.count("mem-flag"))
    {
        mopts.mem_flag = result["mem-flag"].as<int>();
    }
        
    mopts.kmerc_output_prefix = graph_prefix(mopts);
    
    main_kmer_count(mopts);

}