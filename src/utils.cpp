#include "utils.h"

Sketch read_min_hashes(const std::string& json_filename) {

    // Open the JSON file
    std::ifstream inputFile(json_filename);

    // Check if the file is open
    if (!inputFile.is_open()) {
        std::cerr << "Could not open the file!" << std::endl;
        return {};
    }

    // Parse the JSON data
    json jsonData;
    inputFile >> jsonData;

    // Access and print values
    std::vector<hash_t> min_hashes = jsonData[0]["signatures"][0]["mins"];
    std::string name = jsonData[0]["name"];
    std::string md5 = jsonData[0]["signatures"][0]["md5sum"];
    int ksize = jsonData[0]["signatures"][0]["ksize"];
    hash_t max_hash = jsonData[0]["signatures"][0]["max_hash"];
    int seed = jsonData[0]["signatures"][0]["seed"];

    // Close the file
    inputFile.close();

    Sketch sketch(min_hashes, json_filename, name, md5, ksize, max_hash, seed);
    return sketch;

}


/*
Is there a way to make this function more efficient?
Say we have n threads
Each thread handles some sketches
To store the hash value h,
we will need to lock mutex_list[ get_mutex_index(h) ]
then push_back the index to the vector
then unlock the mutex
should be many times faster??
*/



void compute_index_from_sketches_one_chunk( int sketch_index_start, int sketch_index_end,
                                        std::vector<Sketch>& sketches,
                                        MultiSketchIndex& multi_sketch_index,
                                        bool show_progress = false) {


    for (int i = sketch_index_start; i < sketch_index_end; i++) {
        if (show_progress) {
            double percentage = 100.0 * (i - sketch_index_start) / (sketch_index_end - sketch_index_start);
            // show percetage progress, only two decimal points
            std::cout << "\rProgress: " << std::fixed << std::setprecision(2) << percentage << "%";
        }
        for (uint j = 0; j < sketches[i].size(); j++) {
            hash_t hash_value = sketches[i][j];
            multi_sketch_index.add_hash(hash_value, i);
        }
    }

    if (show_progress) {
        std::cout << "\rProgress: " << std::fixed << std::setprecision(2) << 100.00 << "%";
        std::cout << std::endl;
    }


}   



void compute_index_from_sketches(std::vector<Sketch>& sketches, 
                                    MultiSketchIndex& multi_sketch_index,
                                    const int num_threads) {
    
    
    // create threads
    int num_sketches = sketches.size();
    int chunk_size = num_sketches / num_threads;
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        int start_index = i * chunk_size;
        int end_index = (i == num_threads - 1) ? num_sketches : (i + 1) * chunk_size;
        bool show_progress = (i == num_threads - 1);
        threads.push_back(std::thread(compute_index_from_sketches_one_chunk, 
                                        start_index, end_index, 
                                        std::ref(sketches), std::ref(multi_sketch_index), show_progress));
    }

    // join threads
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }


}



void get_sketch_paths(const std::string& filelist, std::vector<std::string>& sketch_paths) {
    // the filelist is a file, where each line is a path to a sketch file
    std::ifstream file(filelist);
    if (!file.is_open()) {
        std::cerr << "Could not open the filelist: " << filelist << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        sketch_paths.push_back(line);
    }
}



void read_sketches_one_chunk(int start_index, int end_index, 
                            std::vector<std::string>& sketch_paths,
                            std::vector<Sketch>& sketches,
                            std::mutex& mutex_count_empty_sketch,
                            std::vector<int>& empty_sketch_ids) {

    for (int i = start_index; i < end_index; i++) {
        auto min_hashes = read_min_hashes(sketch_paths[i]);
        sketches[i] = min_hashes;
        if (sketches[i].size() == 0) {
            mutex_count_empty_sketch.lock();
            empty_sketch_ids.push_back(i);
            mutex_count_empty_sketch.unlock();
        }
    }
}



void read_sketches(std::vector<std::string>& sketch_paths,
                        std::vector<Sketch>& sketches, 
                        std::vector<int>& empty_sketch_ids, 
                        const uint num_threads) {

    uint num_sketches = sketch_paths.size();
    for (uint i = 0; i < num_sketches; i++) {
        sketches.push_back( Sketch() );
    }

    std::mutex mutex_count_empty_sketch;
    int chunk_size = num_sketches / num_threads;
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        int start_index = i * chunk_size;
        int end_index = (i == num_threads - 1) ? num_sketches : (i + 1) * chunk_size;
        threads.push_back(std::thread(read_sketches_one_chunk, 
                                        start_index, end_index, 
                                        std::ref(sketch_paths), std::ref(sketches), 
                                        std::ref(mutex_count_empty_sketch), 
                                        std::ref(empty_sketch_ids)));
    }
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }
    
}




void show_empty_sketches(const std::vector<int>& empty_sketch_ids) {
    int count_empty_sketch = empty_sketch_ids.size();
    std::cout << "Number of empty sketches: " << count_empty_sketch << std::endl;
    if (count_empty_sketch == 0) {
        return;
    }
    std::cout << "Empty sketch ids: ";
    for (int i : empty_sketch_ids) {
        std::cout << i << " ";
    }
    std::cout << std::endl;
}




void compute_intersection_matrix_by_sketches(int query_sketch_start_index, int query_sketch_end_index, 
                                            int thread_id, std::string out_dir, 
                                            int pass_id, int negative_offset,
                                            std::vector<Sketch>& sketches_query,
                                            std::vector<Sketch>& sketches_ref,
                                            MultiSketchIndex& multi_sketch_index_ref,
                                            int** intersectionMatrix, 
                                            double containment_threshold,
                                            std::vector<std::vector<int>>& similars,
                                            bool show_progress = false) {
    
    const int num_sketches_ref = sketches_ref.size();
    const int num_sketches_query = sketches_query.size();

    // process the sketches in the range [sketch_start_index, sketch_end_index)
    if (show_progress) {
        std::cout << "Computation progress: 0.00%";
    }
    for (uint i = query_sketch_start_index; i < query_sketch_end_index; i++) {
        for (int j = 0; j < sketches_query[i].size(); j++) {
            hash_t hash = sketches_query[i][j];
            if (!multi_sketch_index_ref.hash_exists(hash)) {
                continue;
            }
            std::vector<int> ref_sketch_indices = multi_sketch_index_ref.get_sketch_indices(hash);
            for (uint k = 0; k < ref_sketch_indices.size(); k++) {
                intersectionMatrix[i-negative_offset][ref_sketch_indices[k]]++;
            }
        }
        if (show_progress) {
            double percentage = 100.0 * (i - query_sketch_start_index) / (query_sketch_end_index - query_sketch_start_index);
            // show percetage progress, only two decimal points
            std::cout << "\rComputation progress: " << std::fixed << std::setprecision(2) << percentage << "%";
        }
    }
    if (show_progress) {
        std::cout << "\rComputation progress: " << std::fixed << std::setprecision(2) << 100.00 << "%";
        std::cout << std::endl;
    }

    // write the similarity values to file. filename: out_dir/passid_threadid.txt, where id is thread id in 3 digits
    std::string id_in_three_digits_str = std::to_string(thread_id);
    while (id_in_three_digits_str.size() < 3) {
        id_in_three_digits_str = "0" + id_in_three_digits_str;
    }
    std::string pass_id_str = std::to_string(pass_id);
    std::string filename = out_dir + "/" + pass_id_str + "_" + id_in_three_digits_str + ".txt";
    std::ofstream outfile(filename);

    // check if the file is open
    if (!outfile.is_open()) {
        std::cerr << "Could not open the file: " << filename << std::endl;
        exit(1);
    }

    // only write the values if larger than the threshold
    if (show_progress) {
        std::cout << "Writing progress: 0.00%";
    }
    for (int i = query_sketch_start_index; i < query_sketch_end_index; i++) {
        for (uint j = 0; j < num_sketches_ref; j++) {
            // if nothing in the intersection, then skip
            if (intersectionMatrix[i-negative_offset][j] == 0) {
                continue;
            }

            // if either of the sketches is empty, then skip
            if (sketches_query[i].size() == 0 || sketches_ref[j].size() == 0) {
                continue;
            }

            // if the divisor in the jaccard calculation is 0, then skip
            if (sketches_query[i].size() + sketches_ref[j].size() - intersectionMatrix[i-negative_offset][j] == 0) {
                continue;
            }

            double jaccard = 1.0 * intersectionMatrix[i-negative_offset][j] / ( sketches_query[i].size() + sketches_ref[j].size() - intersectionMatrix[i-negative_offset][j] );
            double containment_i_in_j = 1.0 * intersectionMatrix[i-negative_offset][j] / sketches_query[i].size();
            double containment_j_in_i = 1.0 * intersectionMatrix[i-negative_offset][j] / sketches_ref[j].size();
            
            // containment_i_in_j is the containment of query in target, i is the query
            if (containment_i_in_j < containment_threshold) {
                continue;
            }

            std::string query_name = sketches_query[i].name;
            std::string ref_name = sketches_ref[j].name;
            std::string query_md5 = sketches_query[i].md5;
            std::string ref_md5 = sketches_ref[j].md5;

            // write i, query_name, query_md5, j, ref_name, ref_md5, jaccard, containment_i_in_j, containment_j_in_i
            outfile << i << ",\"" << query_name << "\"," << query_md5 << "," << j << ",\"" << ref_name << "\"," << ref_md5 << "," << jaccard << "," << containment_i_in_j << "," << containment_j_in_i << std::endl;
            
            similars[i].push_back(j);
        }
        if (show_progress) {
            double percentage = 100.0 * (i - query_sketch_start_index) / (query_sketch_end_index - query_sketch_start_index);
            // show percetage progress, only two decimal points
            std::cout << "\rWriting progress: " << std::fixed << std::setprecision(2) << percentage << "%";
        }
    }

    if (show_progress) {
        std::cout << "\rWriting progress: " << std::fixed << std::setprecision(2) << 100.00 << "%";
        std::cout << std::endl;
    }

    outfile.close();

}



void compute_intersection_matrix(std::vector<Sketch>& sketches_query,
                                std::vector<Sketch>& sketches_ref, 
                                MultiSketchIndex& multi_sketch_index_ref,
                                std::string& out_dir, 
                                std::vector<std::vector<int>>& similars,
                                double containment_threshold,
                                int num_passes, int num_threads) {
    
    int num_sketches_query = sketches_query.size();
    int num_sketches_ref = sketches_ref.size();

    // allocate memory for the intersection matrix
    int num_query_sketches_each_pass = ceil(1.0 * num_sketches_query / num_passes);
    int** intersectionMatrix = new int*[num_query_sketches_each_pass + 1];
    for (int i = 0; i < num_query_sketches_each_pass + 1; i++) {
        intersectionMatrix[i] = new int[num_sketches_ref];
    }

    // allocate memory for the similars if not already allocated
    if (similars.size() != num_sketches_query) {
        similars.clear();
        similars.resize(num_sketches_query);
    }
    

    for (int pass_id = 0; pass_id < num_passes; pass_id++) {
        // set zeros in the intersection matrix
        for (int i = 0; i < num_query_sketches_each_pass+1; i++) {
            for (uint j = 0; j < num_sketches_ref; j++) {
                intersectionMatrix[i][j] = 0;
            }
        }

        // prepare the indices which will be processed in this pass
        int sketch_idx_start_this_pass = pass_id * num_query_sketches_each_pass;
        int sketch_idx_end_this_pass = (pass_id == num_passes - 1) ? num_sketches_query : (pass_id + 1) * num_query_sketches_each_pass;
        int negative_offset = pass_id * num_query_sketches_each_pass;
        int num_query_sketches_this_pass = sketch_idx_end_this_pass - sketch_idx_start_this_pass;
        
        // create threads
        std::vector<std::thread> threads;
        int chunk_size = num_query_sketches_this_pass / num_threads;
        for (int i = 0; i < num_threads; i++) {
            int start_query_index_this_thread = sketch_idx_start_this_pass + i * chunk_size;
            int end_query_index_this_thread = (i == num_threads - 1) ? sketch_idx_end_this_pass : sketch_idx_start_this_pass + (i + 1) * chunk_size;
            // use emplace_back to avoid copy, set the last thread to show progress
            std::thread t(compute_intersection_matrix_by_sketches, 
                            start_query_index_this_thread, end_query_index_this_thread, 
                            i, out_dir, pass_id, negative_offset,
                            std::ref(sketches_query), std::ref(sketches_ref), 
                            std::ref(multi_sketch_index_ref), 
                            intersectionMatrix, containment_threshold, 
                            std::ref(similars), i == num_threads - 1);
            threads.emplace_back(std::move(t));
        }

        // join threads
        for (int i = 0; i < num_threads; i++) {
            threads[i].join();
        }

        // show progress
        std::cout << "Pass " << pass_id+1 << "/" << num_passes << " done." << std::endl;
    }

    // free the memory allocated for the intersection matrix
    for (int i = 0; i < num_query_sketches_each_pass + 1; i++) {
        delete[] intersectionMatrix[i];
    }
    delete[] intersectionMatrix;
}

