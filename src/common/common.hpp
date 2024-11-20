#ifndef COMMON_H_
#define COMMON_H_

#include <iostream>
#include <stdint.h>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <stdexcept>
#include <cstdlib>
#include <unistd.h>

#include "mpienv/mpienv.hpp"

using namespace std;

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

long long detect_available_mem();


class GlobalOptions {
public:
    string out_dir = "";
    string temp_dir = "";
    bool test_mode = false;
    bool continue_mode = false;
    bool force_overwrite = false;
    double memory = 0.9;
    int min_contig_len = 200;
    int k = 21;
    int k_max = 141;
    int k_step = 10;
    vector<int> k_list = {21, 29, 39, 59, 79, 99, 119, 141};
    bool auto_k = true;
    bool set_list_by_min_max_step = false;
    int min_count = 2;
    bool has_popcnt = true;
    bool hw_accel = true;
    int max_tip_len = -1;
    bool no_mercy = false;
    bool no_local = false;
    int bubble_level = 2;
    int merge_len = 20;
    double merge_similar = 0.95;
    int prune_level = 2;
    int prune_depth = 2;
    int num_cpu_threads = 0;
    double disconnect_ratio = 0.1;
    double low_local_ratio = 0.2;
    int cleaning_rounds = 5;
    bool keep_tmp_files = false;
    int mem_flag = 1;
    string out_prefix = "";
    bool kmin_1pass = false;
    vector<string> pe1;
    vector<string> pe2;
    vector<string> pe12;
    vector<string> se;
    string presets = "";
    string kmerc_output_prefix = "out";
    bool verbose = false;
    MPIEnviroment mpipars;

    string log_file_name() const {
        if (out_prefix.empty()) {
            return out_dir + "/log";
        } else {
            return out_dir + "/" + out_prefix + ".log";
        }
    }

    string option_file_name() const {
        return out_dir + "/options.json";
    }

    string contig_dir() const {
        return out_dir + "/intermediate_contigs";
    }

    string read_lib_path() const {
        return temp_dir + "/reads.lib";
    }

    long long host_mem() const {
        if (memory <= 0) {
            throw runtime_error("Please specify a positive number for -m flag.");
        } else if (memory < 1) {
            try {
                long long total_mem = detect_available_mem();
                return static_cast<long long>(std::floor(total_mem * memory));
            } catch (...) {
                throw runtime_error("Failed to detect available memory size, "
                                    "please specify memory size in bytes via -m option");
            }
        } else {
            return static_cast<long long>(std::floor(memory));
        }
    }
};




#endif