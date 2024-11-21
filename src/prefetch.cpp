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

    int num_hashes_in_ref = 0;
    for (hash_t hash_value : query_sketch) {
        if ( ref_index.hash_exists(hash_value) ) {
            num_hashes_in_ref++;
        }
   }

    cout << "Number of hashes in ref: " << num_hashes_in_ref << endl;

    vector<size_t> num_intersection_values(ref_sketches.size(), 0);
    for (hash_t hash_value : query_sketch) {
        vector<int> matching_ref_ids = ref_index.get_sketch_indices(hash_value);
        for (int ref_id : matching_ref_ids) {
            num_intersection_values[ref_id]++;
        }
    }

    // sort the ref sketches by the number of intersections
    vector<size_t> sorted_indices(ref_sketches.size());
    iota(sorted_indices.begin(), sorted_indices.end(), 0);
    sort(sorted_indices.begin(), sorted_indices.end(), [&](size_t i, size_t j) {
        return num_intersection_values[i] > num_intersection_values[j];
    });

    // show fisrt 10 ref sketches and their intersection values
    cout << "First 10 ref sketches and their intersection values" << endl;
    for (int i = 0; i < 10; i++) {
        cout << "Ref sketch id: " << sorted_indices[i] << " Intersection value: " << num_intersection_values[sorted_indices[i]] << endl;
    }

    return 0;

}