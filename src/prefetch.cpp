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

    read_min_hashes(arguments.query_path);
    get_sketch_paths(arguments.ref_filelist, ref_sketch_paths);
    read_sketches(ref_sketch_paths, ref_sketches, empty_sketch_ids, arguments.number_of_threads);
    compute_index_from_sketches(ref_sketches, ref_index, arguments.number_of_threads);

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

    return 0;

}