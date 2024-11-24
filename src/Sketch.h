// a sketch is a vector of hash_t, sorted in ascending order
// other attributes are: file path, and name


# include <iostream>
# include <vector>
# include <string>


#ifndef HASH_T
#define HASH_T
typedef unsigned long long int hash_t;
#endif


#ifndef SKETCH_H
#define SKETCH_H

class Sketch {
    public:
        std::vector<hash_t> hashes;
        std::string file_path;
        std::string name;
        std::string md5;
        int ksize;
        hash_t max_hash;
        int seed;


        Sketch(std::vector<hash_t> hashes, std::string file_path, std::string name, std::string md5, int ksize, hash_t max_hash, int seed) {
            this->hashes = hashes;
            this->file_path = file_path;
            this->name = name;
            this->md5 = md5;
            this->ksize = ksize;
            this->max_hash = max_hash;
            this->seed = seed;
        }

        Sketch() {
            this->hashes = std::vector<hash_t>();
            this->file_path = "";
            this->name = "";
            this->md5 = "";
            this->ksize = 0;
            this->max_hash = 0;
            this->seed = 0;
        }

        void show();

        void show_hashes() {
            for (hash_t hash : this->hashes) {
                std::cout << hash << " ";
            }
            std::cout << std::endl;
        }

        size_t size() {
            return this->hashes.size();
        }

        bool empty() {
            return this->hashes.empty();
        }

        hash_t operator[](int i) {
            return this->hashes[i];
        }

};

#endif