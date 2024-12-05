/*
Load sketches present in a filelist,
compute the index from the sketches,
compute all v all containment values,
write the results to a file.
*/

#include <iostream>
#include <vector>

#include "argparse.hpp"
#include "json.hpp"
#include "utils.h"
#include "MultiSketchIndex.h"

using namespace std;
using json = nlohmann::json;

struct Arguments {
    string filelist;
    string working_dir;
    string output_filename;
    double containment_threshold;
    int number_of_threads;
    int num_hashtables;
    int num_passes;
};


typedef Arguments Arguments;



void do_compare(Arguments& args) {
    // data structures
    vector<string> all_sketch_paths;
    vector<Sketch> all_sketches;
    vector<int> empty_sketch_ids;
    MultiSketchIndex all_sketch_index(args.num_hashtables);

    // Read the sketches
    auto read_start = chrono::high_resolution_clock::now();
    cout << "Reading all sketches using " << args.number_of_threads << " threads" << endl;
    get_sketch_paths(args.filelist, all_sketch_paths);
    read_sketches(all_sketch_paths, 
                    all_sketches, 
                    empty_sketch_ids, 
                    args.number_of_threads);
    auto read_end = chrono::high_resolution_clock::now();
    auto read_duration = chrono::duration_cast<chrono::seconds>(read_end - read_start);
    cout << "Reading completed in " << read_duration.count() << " seconds." << endl;

    // Compute the index from the sketches
    auto start = chrono::high_resolution_clock::now();
    cout << "Building an index on all the kmers... (will take some time)" << endl;
    compute_index_from_sketches(all_sketches, 
                                all_sketch_index, 
                                args.number_of_threads);
    auto end = chrono::high_resolution_clock::now();
    auto duration_in_seconds = chrono::duration_cast<chrono::seconds>(end - start);
    cout << "Index building completed in " << duration_in_seconds.count() << " seconds." << endl;

    // Compute all v all containment values
    cout << "Computing all v all containment values..." << endl;
    vector<vector<int>> similars;
    auto start_compute = chrono::high_resolution_clock::now();
    compute_intersection_matrix(all_sketches, 
                                all_sketches, 
                                all_sketch_index, 
                                args.working_dir, 
                                similars, 
                                args.containment_threshold, 
                                args.num_passes,
                                args.number_of_threads);
    auto end_compute = chrono::high_resolution_clock::now();
    auto duration_compute = chrono::duration_cast<chrono::seconds>(end_compute - start_compute);
    cout << "Containment values computed in " << duration_compute.count() << " seconds." << endl;

    // Write the results to a file
    cout << "Writing the results to " << args.output_filename << endl;
    // combine the files in out_dir/a_xyz.txt to args.output_filename, where 0 <= a < args.num_passes,
    // and 0 <= xyz < args.number_of_threads
    vector<string> files_to_combine;
    for (int i = 0; i < args.num_passes; i++) {
        for (int j = 0; j < args.number_of_threads; j++) {
            string pass_id_str = to_string(i);
            string thread_id_str = to_string(j);
            while (thread_id_str.size() < 3) {
                thread_id_str = "0" + thread_id_str;
            }
            string filename = args.working_dir + "/" + pass_id_str + "_" + thread_id_str + ".txt";
            files_to_combine.push_back(filename);
        }
    }
    // combining command: cat 
    string combine_command = "cat ";
    for (string filename : files_to_combine) {
        combine_command += filename + " ";
    }
    combine_command += " > " + args.output_filename;
    // call the system command and check if it is successful
    if (system(combine_command.c_str()) != 0) {
        cerr << "Error in combining the files." << endl;
        exit(1);
    }
    cout << "Results written to " << args.output_filename << endl;

    // Clean up
    cout << "Cleaning up and exiting... (may take some time)" << endl;

}



void parse_args(int argc, char** argv, Arguments &arguments) {

    argparse::ArgumentParser parser("compare");

    parser.add_argument("filelist")
        .help("The path to the file containing the paths to the sketches")
        .required()
        .store_into(arguments.filelist);

    parser.add_argument("working_dir")
        .help("The directory where smaller files will be stored")
        .required()
        .store_into(arguments.working_dir);

    parser.add_argument("output_filename")
        .help("The path to the output file")
        .required()
        .store_into(arguments.output_filename);

    parser.add_argument("-c", "--containment-threshold")
        .help("The containment threshold above which outputs are written")
        .scan<'g', double>()
        .default_value(0.5)
        .store_into(arguments.containment_threshold);

    parser.add_argument("-t", "--threads")
        .help("The number of threads to use")
        .scan<'i', int>()
        .default_value(1)
        .store_into(arguments.number_of_threads);
    
    parser.add_argument("-n", "--num-hashtables")
        .help("The number of hash tables to use")
        .scan<'i', int>()
        .default_value(4096)
        .store_into(arguments.num_hashtables);

    parser.add_argument("-p", "--num-passes")
        .help("The number of passes to use")
        .scan<'i', int>()
        .default_value(1)
        .store_into(arguments.num_passes);
    
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
    cout << "*   Filelist: " << args.filelist << endl;
    cout << "*   Working directory: " << args.working_dir << endl;
    cout << "*   Output filename: " << args.output_filename << endl;
    cout << "*   Containment threshold: " << args.containment_threshold << endl;
    cout << "*   Number of threads: " << args.number_of_threads << endl;
    cout << "*   Number of hash tables: " << args.num_hashtables << endl;
    cout << "*   Number of passes: " << args.num_passes << endl;
    cout << "*" << endl;
    cout << "**************************************" << endl;
}



int main( int argc, char** argv ) {

    Arguments arguments;
    parse_args(argc, argv, arguments);
    show_args(arguments);
    do_compare(arguments);

    return 0;

}