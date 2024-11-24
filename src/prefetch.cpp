#include "argparse.hpp"
#include "json.hpp"
#include "utils.h"
#include "MultiSketchIndex.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>
#include <tuple>
#include <algorithm>
#include <unordered_set>
#include <set>
#include <thread>
#include <mutex>
#include <cmath>
#include <limits>
#include <iomanip>
#include <sstream>
#include <random>
#include <exception>

#include <sys/resource.h>
#include <cstdlib>

using namespace std;
using json = nlohmann::json;


struct Arguments {
    string query_path;
    string ref_filelist;
    string output_filename;
    int number_of_threads;
    int threshold_bp;
};


typedef Arguments Arguments;
typedef unsigned long long int hash_t;


void parse_args(int argc, char** argv, Arguments &arguments) {
    argparse::ArgumentParser parser("prefetch: find all matching refs for a query sketch");

    parser.add_argument("query_path")
        .help("The path to the query sketch")
        .required()
        .store_into(arguments.query_path);

    parser.add_argument("ref_filelist")
        .help("The path to the file containing the paths to the reference sketches")
        .required()
        .store_into(arguments.ref_filelist);

    parser.add_argument("output_filename")
        .help("The path to the output file")
        .required()
        .store_into(arguments.output_filename);

    parser.add_argument("-t", "--threads")
        .help("The number of threads to use")
        .scan<'i', int>()
        .default_value(1)
        .store_into(arguments.number_of_threads);
    
    parser.add_argument("-b", "--threshold-bp")
        .help("The threshold in base pairs")
        .scan<'i', int>()
        .default_value(50)
        .store_into(arguments.threshold_bp);

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        std::cout << err.what() << std::endl;
        std::cout << parser;
        exit(1);
    }
}



void show_args(Arguments &args) {
    cout << "**************************************" << endl;
    cout << "*" << endl;
    cout << "*   Query path: " << args.query_path << endl;
    cout << "*   Ref filelist: " << args.ref_filelist << endl;
    cout << "*   Output filename: " << args.output_filename << endl;
    cout << "*   Number of threads: " << args.number_of_threads << endl;
    cout << "*   Threshold in base pairs: " << args.threshold_bp << endl;
    cout << "*" << endl;
    cout << "**************************************" << endl;
} 







int main(int argc, char** argv) {

    Arguments arguments;
    parse_args(argc, argv, arguments);
    show_args(arguments);

    Sketch query_sketch; 
    vector<string> ref_sketch_paths;
    vector<Sketch> ref_sketches;
    vector<int> empty_sketch_ids;
    MultiSketchIndex ref_index;

    // Read the query sketch and the reference sketches
    auto read_start = chrono::high_resolution_clock::now();
    cout << "Reading all reference sketches and the query sketch using " << arguments.number_of_threads << " threads" << endl;
    query_sketch = read_min_hashes(arguments.query_path);
    get_sketch_paths(arguments.ref_filelist, ref_sketch_paths);
    read_sketches(ref_sketch_paths, ref_sketches, empty_sketch_ids, arguments.number_of_threads);
    auto read_end = chrono::high_resolution_clock::now();
    auto read_duration = chrono::duration_cast<chrono::seconds>(read_end - read_start);
    cout << "Completed reading one query and " << ref_sketches.size() << " reference sketches." << endl;
    cout << "Reading completed in " << read_duration.count() << " seconds." << endl;

    size_t num_total_hashes_in_ref = 0;
    for (Sketch ref_sketch : ref_sketches) {
        num_total_hashes_in_ref += ref_sketch.size();
    }

    // show num of hashes in query and ref
    cout << "Number of kmers in query: " << query_sketch.size() << endl;
    cout << "Number of kmers in all the references: " << num_total_hashes_in_ref << endl;

    // Compute the index from the reference sketches
    auto start = chrono::high_resolution_clock::now();
    cout << "Building an index on all the reference kmers... (will take some time)" << endl;
    compute_index_from_sketches(ref_sketches, ref_index, arguments.number_of_threads);
    auto end = chrono::high_resolution_clock::now();
    auto duration_in_seconds = chrono::duration_cast<chrono::seconds>(end - start);
    cout << "Index building completed in " << duration_in_seconds.count() << " seconds." << endl;

    // show num of hashes in ref
    cout << "Number of distinct kmers in the references: " << ref_index.size() << endl;

    cout << "Now searching the query kmers against the references..." << endl;

    vector<hash_t> query_hashes_present_in_ref;
    for (hash_t hash_value : query_sketch.hashes) {
        if ( ref_index.hash_exists(hash_value) ) {
            query_hashes_present_in_ref.push_back(hash_value);
        }
    }
    cout << "Number of kmers in query present in the references: " << query_hashes_present_in_ref.size() << endl;

    size_t* num_intersection_values = new size_t[ref_sketches.size()];
    size_t* num_intersection_values_orig = new size_t[ref_sketches.size()];
    for (size_t i = 0; i < ref_sketches.size(); i++) {
        num_intersection_values[i] = 0;
        num_intersection_values_orig[i] = 0;
    }
    for (hash_t hash_value : query_hashes_present_in_ref) {
        vector<int> matching_ref_ids = ref_index.get_sketch_indices(hash_value);
        for (int ref_id : matching_ref_ids) {
            num_intersection_values[ref_id]++;
            num_intersection_values_orig[ref_id]++;
        }
    }

    // build an unordered map of the hashes in the query sketch
    unordered_map<hash_t, bool> query_hash_map;
    for (hash_t hash_value : query_sketch.hashes) {
        query_hash_map[hash_value] = true;
    }

    int num_iterations = 0;
    vector<tuple<int, size_t, size_t>> results;

    while( true ) {
        
        // find the id of the ref sketch with the maximum number of intersections
        size_t max_intersection_value = 0;
        size_t max_intersection_ref_id = 0;

        for (size_t i = 0; i < ref_sketches.size(); i++) {
            if (num_intersection_values[i] > max_intersection_value) {
                max_intersection_value = num_intersection_values[i];
                max_intersection_ref_id = i;
            } else if (num_intersection_values[i] == max_intersection_value) {
                if ( ref_sketches[i].size() >= ref_sketches[max_intersection_ref_id].size() ) {
                    max_intersection_value = num_intersection_values[i];
                    max_intersection_ref_id = i;
                } else if ( ref_sketches[i].size() == ref_sketches[max_intersection_ref_id].size() ) {
                    if ( ref_sketches[i].name.compare(ref_sketches[max_intersection_ref_id].name) <= 0 ) {
                        max_intersection_value = num_intersection_values[i];
                        max_intersection_ref_id = i;
                    }
                }
            }
        }

        if (max_intersection_value < arguments.threshold_bp) {
            cout << "Matched " << max_intersection_ref_id+1 << "-th genome, Num overlap now: " << max_intersection_value << endl;
            cout << "Num overlap is less than the threshold of " << arguments.threshold_bp << " bp. Stopping gather..." << endl;
            break;
        }

        // show match id and match value
        cout << "Matched " << max_intersection_ref_id+1 << "-th genome, Num overlap now: " << max_intersection_value << endl;
        results.push_back(make_tuple(max_intersection_ref_id, max_intersection_value, num_intersection_values_orig[max_intersection_ref_id]));

        // remove the ref sketch with the maximum number of intersections
        for (hash_t hash_value : ref_sketches[max_intersection_ref_id].hashes) {
            vector<int> removed_ids = ref_index.remove_hash(hash_value);
            if (query_hash_map.find(hash_value) != query_hash_map.end()) {
                for (int ref_id : removed_ids) {
                    num_intersection_values[ref_id]--;
                }
                query_hash_map.erase(hash_value);
            }
        }

    }

    // write the results to the output file
    cout << "Writing the results to " << arguments.output_filename << "..." << endl;
    ofstream output_file(arguments.output_filename);
    output_file << "ref_id,num_overlap,num_overlap_orig,name,md5" << endl;
    
    for (tuple<int, size_t, size_t> result : results) {
        
        int sketch_index = get<0>(result);
        int num_overlap = get<1>(result);
        int num_overlap_orig = get<2>(result);
        string file_path = ref_sketches[sketch_index].file_path;
        string name = ref_sketches[sketch_index].name;
        string md5 = ref_sketches[sketch_index].md5;
        
        output_file << sketch_index << "," << num_overlap << "," << num_overlap_orig << ",\"" << name << "\"," << md5 << endl;
    }

    output_file.close();

    cout << "Results written to " << arguments.output_filename << endl;
    cout << "Cleaning up and exiting... (may take some time)" << endl;

    delete[] num_intersection_values;
    delete[] num_intersection_values_orig;
    struct rlimit limit = {0, 0};
    setrlimit(RLIMIT_CORE, &limit);
    terminate();

    return 0;

}