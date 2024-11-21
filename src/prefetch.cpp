#include "argparse.hpp"
#include "json.hpp"
#include "utils.h"
#include "MultiSketchIndex.h"

using namespace std;
using json = nlohmann::json;


struct Arguments {
    string query_path;
    string ref_filelist;
    string output_filename;
    int number_of_threads;
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

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        std::cout << err.what() << std::endl;
        std::cout << parser;
        exit(1);
    }
}



void show_args(Arguments &args) {
    cout << "Query path: " << args.query_path << endl;
    cout << "Ref filelist: " << args.ref_filelist << endl;
    cout << "Output filename: " << args.output_filename << endl;
    cout << "Number of threads: " << args.number_of_threads << endl;
} 







int main(int argc, char** argv) {

    Arguments arguments;
    parse_args(argc, argv, arguments);
    show_args(arguments);

    vector<hash_t> query_sketch; 
    vector<string> ref_sketch_paths;
    vector<vector<hash_t>> ref_sketches;
    vector<int> empty_sketch_ids;
    MultiSketchIndex ref_index;

    // Read the query sketch and the reference sketches
    cout << "Reading sketches" << endl;
    query_sketch = read_min_hashes(arguments.query_path);
    get_sketch_paths(arguments.ref_filelist, ref_sketch_paths);
    read_sketches(ref_sketch_paths, ref_sketches, empty_sketch_ids, arguments.number_of_threads);
    cout << "Sketch reading done" << endl;

    // show num of hashes in query
    cout << "Num of hashes in query: " << query_sketch.size() << endl;

    // Compute the index from the reference sketches
    cout << "Computing index" << endl;
    compute_index_from_sketches(ref_sketches, ref_index, arguments.number_of_threads);
    cout << "Index computation done" << endl;

    // show num of hashes in ref
    cout << "Num of hashes in ref: " << ref_index.size() << endl;

    vector<hash_t> query_hashes_present_in_ref;
    for (hash_t hash_value : query_sketch) {
        if ( ref_index.hash_exists(hash_value) ) {
            query_hashes_present_in_ref.push_back(hash_value);
        }
    }

    cout << "Number of hashes in query present in ref: " << query_hashes_present_in_ref.size() << endl;

    size_t* num_intersection_values = new size_t[ref_sketches.size()];
    for (hash_t hash_value : query_hashes_present_in_ref) {
        vector<int> matching_ref_ids = ref_index.get_sketch_indices(hash_value);
        for (int ref_id : matching_ref_ids) {
            num_intersection_values[ref_id]++;
        }
    }

    // build an unordered map of the hashes in the query sketch
    unordered_map<hash_t, bool> query_hash_map;
    for (hash_t hash_value : query_sketch) {
        query_hash_map[hash_value] = true;
    }

    int num_iterations = 0;

    while( true ) {
        
        // find the id of the ref sketch with the maximum number of intersections
        size_t max_intersection_value = 0;
        size_t max_intersection_ref_id = 0;
        for (size_t i = 0; i < ref_sketches.size(); i++) {
            if (num_intersection_values[i] > max_intersection_value) {
                max_intersection_value = num_intersection_values[i];
                max_intersection_ref_id = i;
            }
        }

        if (max_intersection_value == 0) {
            break;
        }

        // show match id and match value
        cout << "Match id: " << max_intersection_ref_id << " Num overlap: " << max_intersection_value << endl;

        // remove the ref sketch with the maximum number of intersections
        for (hash_t hash_value : ref_sketches[max_intersection_ref_id]) {
            vector<int> removed_ids = ref_index.remove_hash(hash_value);
            if (query_hash_map.find(hash_value) != query_hash_map.end()) {
                for (int ref_id : removed_ids) {
                    num_intersection_values[ref_id]--;
                }
            }
        }

        num_iterations++;

        if (num_iterations == 10000) {
            break;
        }

    }


    return 0;

}