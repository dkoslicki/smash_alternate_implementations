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
    int number_of_threads;
    int num_hashtables;
};


typedef Arguments Arguments;


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
    cout << "*   Number of threads: " << args.number_of_threads << endl;
    cout << "*   Number of hash tables: " << args.num_hashtables << endl;
    cout << "*" << endl;
    cout << "**************************************" << endl;
}



int main( int argc, char** argv ) {

    Arguments arguments;
    parse_args(argc, argv, arguments);

    show_args(arguments);

    return 0;

}