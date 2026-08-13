#include "cmath"
#include "hash_table.hpp"

template <typename T>
HashTable<T>::HashTable(){}

template <typename T>
HashTable<T>::~HashTable(){
    clear();
}

template <typename T>
bool HashTable<T>::HashData::operator == (const HashTable<T>::HashData &HASHDATA){
    return key_ == HASHDATA.key_;
}

template <typename T>
bool HashTable<T>::createTable(uint32_t desiredSize){
    if(table_){
        clear();
    }
    table_ = new std::list<HashData> [desiredSize];
    double tmpPower = std::log2(desiredSize);
    if(tmpPower  - static_cast<int>(tmpPower) != 0.f){
        return false;
    }
    power_ = static_cast<int>(tmpPower);
    size_ = desiredSize;
    return true;
}

template <typename T>
bool HashTable<T>::insertNode(uint32_t key, T data){
    if(!table_){
        return false;
    }
    HashData hashData;
    hashData.key_ = key;
    hashData.data_ = data;
    table_[hash(key)].push_back(hashData);
    if(is_rehashing_){
        return true;
    }
    data_count_++;
    if(!checkRehash()){
        return false;
    }
    return true;
}

template <typename T>
bool HashTable<T>::deleteNode(uint32_t key){
    if(!table_){
        return false;
    }
    T *tmpData = getNode(key);
    if(!tmpData){
        return false;
    }
    HashData hashData;
    hashData.key_ = key;
    hashData.data_ = *tmpData;
    std::size_t old_size = table_[hash(key)].size();
    table_[hash(key)].remove(hashData);
    data_count_ -= (old_size - table_[hash(key)].size());
    return true;
}

template <typename T>
bool HashTable<T>::searchNode(uint32_t key){
    if(!table_){
        return false;
    }
    for(auto it = table_[hash(key)].begin(); it != table_[hash(key)].end(); it++){
        if(it->key_ == key){
            return true;
        }
    }
    return false;
}

template <typename T>
T *HashTable<T>::getNode(uint32_t key){
    if(!table_){
        return nullptr;
    }
    for(auto it = table_[hash(key)].begin(); it != table_[hash(key)].end(); it++){
        if(it->key_ == key){
            return &it->data_;
        }
    }
    return nullptr;
}

template <typename T>
std::list<typename HashTable<T>::HashData> *HashTable<T>::getList(uint32_t key){
    if(!table_){
        return nullptr;
    }
    return &table_[hash(key)];
}

template <typename T>
void HashTable<T>::resetListPtr(){
    current_node_ = 0;
}

template <typename T>
bool HashTable<T>::advanceListPtr(){
    if(current_node_ + 1 >= size_){
        return false;
    }
    current_node_++;
    return true;
}

template <typename T>
std::list <typename HashTable<T>::HashData> *HashTable<T>::getListPtr(){
    return &table_[current_node_];
}

template <typename T>
bool HashTable<T>::checkRehash(){
    if(size_ == 0){
        return false;
    }
    float loadFactor = static_cast<float>(data_count_ / size_);
    if(loadFactor >= LOAD_FACTOR){
        if(!rehash()){
            return false;
        }
    }
    return true;
}

template <typename T>
bool HashTable<T>::rehash(){
    is_rehashing_ = true;
    std::size_t oldDataCount = data_count_;
    if(!table_) {
        return false;
    }
    std::list<HashData> *oldTable = table_;
    std::list<HashData> *newTable = new std::list<HashData> [size_ * 2];
    std::size_t oldSize = size_;
    size_ *= 2;
    power_ ++;
    table_ = newTable;

    for(unsigned int i = 0; i < oldSize; i++){
        for(auto it = oldTable[i].begin(); it != oldTable[i].end(); it++){
            insertNode(it->key_, it->data_);
        }
    }
    delete [] oldTable;
    is_rehashing_ = false;
    data_count_ = oldDataCount;
    return true;
}

template <typename T>
unsigned int HashTable<T>::hash(uint32_t key){
    unsigned int hashValue = hashFunction(key);
    return hashValue;
}

template <typename T>
unsigned int HashTable<T>::hashFunction(uint32_t key){
    return static_cast<int>((key * 0x9E3779B97F4A7C15) >> (64 - power_));
}

template <typename T>
std::size_t HashTable<T>::getSize(){
    if(!table_){
        return 0;
    }
    else{
        return size_;
    }
}

template <typename T>
std::size_t HashTable<T>::getDataCount(){
    return data_count_;
}

template <typename T>
void HashTable<T>::clear(){
    if(table_){
        size_ = 0;
        power_ = 0;
        data_count_ = 0;
        delete [] table_;
        table_ = nullptr;
    }
}