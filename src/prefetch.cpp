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
    int num_hashtables;
};


typedef Arguments Arguments;
typedef unsigned long long int hash_t;



void do_prefetch(Arguments& args) {
    // data structures
    Sketch query_sketch; 
    vector<string> ref_sketch_paths;
    vector<Sketch> ref_sketches;
    vector<int> empty_sketch_ids;
    MultiSketchIndex ref_index(args.num_hashtables);

    // Read the query sketch and the reference sketches
    auto read_start = chrono::high_resolution_clock::now();
    cout << "Reading all reference sketches and the query sketch using " << args.number_of_threads << " threads" << endl;
    query_sketch = read_min_hashes(args.query_path);
    get_sketch_paths(args.ref_filelist, ref_sketch_paths);
    read_sketches(ref_sketch_paths, ref_sketches, empty_sketch_ids, args.number_of_threads);
    
    // read complete, show time taken
    auto read_end = chrono::high_resolution_clock::now();
    auto read_duration = chrono::duration_cast<chrono::seconds>(read_end - read_start);
    cout << "Completed reading one query and " << ref_sketches.size() << " reference sketches." << endl;
    cout << "Reading completed in " << read_duration.count() << " seconds." << endl;

    // compute the number of hashes in the reference sketches
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
    compute_index_from_sketches(ref_sketches, ref_index, args.number_of_threads);
    auto end = chrono::high_resolution_clock::now();
    auto duration_in_seconds = chrono::duration_cast<chrono::seconds>(end - start);
    cout << "Index building completed in " << duration_in_seconds.count() << " seconds." << endl;

    // show num of hashes in ref
    cout << "Number of distinct kmers in the references: " << ref_index.size() << endl;

    // start prefetch
    cout << "Now searching the query kmers against the reference kmers..." << endl;
    size_t* num_intersection_values = new size_t[ref_sketches.size()];
    for (size_t i = 0; i < ref_sketches.size(); i++) {
        num_intersection_values[i] = 0;
    }

    for (hash_t hash_value : query_sketch.hashes) {
        vector<int> matching_ref_ids = ref_index.get_sketch_indices(hash_value);
        for (int ref_id : matching_ref_ids) {
            num_intersection_values[ref_id]++;
        }
    }
    
    // now sort the ref sketches based on the number of intersections
    vector<tuple<int, size_t>> ref_id_num_intersections;
    for (int i = 0; i < ref_sketches.size(); i++) {
        ref_id_num_intersections.push_back(make_tuple(i, num_intersection_values[i]));
    }

    sort(ref_id_num_intersections.begin(), ref_id_num_intersections.end(), 
        [](const tuple<int, size_t>& a, const tuple<int, size_t>& b) {
            return get<1>(a) > get<1>(b);
        });

    // write the results to the output file
    ofstream outfile(args.output_filename);
    outfile << "ref_id,num_intersections,containment_query_ref,containment_ref_query,jaccard" << endl;
    for (auto ref_id_num_intersection : ref_id_num_intersections) {
        int ref_id = get<0>(ref_id_num_intersection);
        size_t num_intersection = get<1>(ref_id_num_intersection);
        if (num_intersection < args.threshold_bp) {
            break;
        }
        double containment_query_ref = 1.0 * num_intersection / query_sketch.size();
        double containment_ref_query = 1.0 * num_intersection / ref_sketches[ref_id].size();
        double jaccard = 1.0 * num_intersection / (query_sketch.size() + ref_sketches[ref_id].size() - num_intersection);
        outfile << ref_id << "," << num_intersection << "," << containment_query_ref << "," << containment_ref_query << "," << jaccard << endl;
    }

    outfile.close();
    cout << "Results written to " << args.output_filename << endl;
    cout << "Cleaning up and exiting... (may take some time)" << endl;

    // clean up
    delete[] num_intersection_values;
    
}



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

    parser.add_argument("-n", "--num-hashtables")
        .help("The number of hash tables to use")
        .scan<'i', int>()
        .default_value(4096)
        .store_into(arguments.num_hashtables);

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
    cout << "*   Number of hash tables in the index: " << args.num_hashtables << endl;
    cout << "*" << endl;
    cout << "**************************************" << endl;
} 







int main(int argc, char** argv) {

    Arguments arguments;
    parse_args(argc, argv, arguments);
    show_args(arguments);
    do_prefetch(arguments);    

    return 0;

}