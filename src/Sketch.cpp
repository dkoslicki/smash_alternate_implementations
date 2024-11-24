# include "Sketch.h"

void Sketch::show() {
    
    std::cout << "Sketch name: " << this->name << std::endl;
    std::cout << "Sketch file path: " << this->file_path << std::endl;
    std::cout << "Sketch md5: " << this->md5 << std::endl;
    std::cout << "Number of hashes: " << this->hashes.size() << std::endl;
    std::cout << "ksize: " << this->ksize << std::endl;
    std::cout << "max_hash: " << this->max_hash << std::endl;
    std::cout << "seed: " << this->seed << std::endl;
    std::cout << "Hashes: ";
    for (hash_t hash : this->hashes) {
        std::cout << hash << " ";
    }
    std::cout << std::endl;

}
