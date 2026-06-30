#include "cmath"
#include "hash_table.hpp"

template <typename T>
HashTable<T>::HashTable(){
    is_rehashing_ = false;
    table_ = nullptr;
    data_count_ = 0;
    size_ = 0;
    power_ = 0;
    current_node_ = 0;
}

template <typename T>
HashTable<T>::~HashTable(){
    clear();
}

template <typename T>
bool HashTable<T>::createTable(unsigned int desiredSize){
    if(table_ || !desiredSize){
        return false;
    }
    table_ = new(std::nothrow) LinkedList<HashData> [desiredSize];
    if(!table_) {
        return false;
    }
    double tmpPower = std::log2(desiredSize);
    if(tmpPower  - (int)tmpPower != 0.f){
        return false;
    }
    power_ = (int) tmpPower;
    size_ = desiredSize;
    return true;
}

template <typename T>
bool HashTable<T>::insertNode(int key, T data){
    if(!table_){
        return false;
    }
    HashData hashData;
    hashData.key_ = key;
    hashData.data_ = data;
    if(!table_[hash(key)].insertTail(hashData)){
        return false;
    }
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
bool HashTable<T>::deleteNode(int key){
    if(!table_){
        return false;
    }
    if(table_[hash(key)].deleteNode(key)){
        data_count_--;
        return true;
    }
    return false;
}

template <typename T>
bool HashTable<T>::searchNode(int key){
    if(!table_){
        return false;
    }
    if(table_[hash(key)].searchNode(key)){
        return true;
    }
    return false;
}

template <typename T>
const T *HashTable<T>::getNode(int key){
    if(!table_){
        return nullptr;
    }
    table_[hash(key)].resetNodeIndex();
    while(table_[hash(key)].hasNode()){
        HashData tmpData = table_[hash(key)].getNode();
        if(tmpData.key_ == key){
            return &table_[hash(key)].getNode().data_;
        }
        table_[hash(key)].advanceNode();
    }
    return nullptr;
}

template <typename T>
bool HashTable<T>::hasNodes(){
    if(current_node_ + 1 >= size_){
        return false;
    }
    return true;
}

template <typename T>
bool HashTable<T>::hasNode(){
    if(!table_[current_node_].hasNode()){
        return false;
    }
    return true;
}

template <typename T>
bool HashTable<T>::advanceNode(){
    if(!table_[current_node_].advanceNode()){
        if(current_node_ + 1 >= size_){
            return false;
        } else{
            current_node_++;
        }
    } else{
        table_[current_node_].resetNodeIndex();
    }
    return true;
}

template <typename T>
void HashTable<T>::resetNodeIndex(){
    table_[0].resetNodeIndex();
    current_node_ = 0;
}

template <typename T>
const typename HashTable<T>::HashData* HashTable<T>::getNode(){
    return &table_[current_node_].getNode();
}

template <typename T>
bool HashTable<T>::checkRehash(){
    if(!size_){
        return false;
    }
    float loadFactor = (float) data_count_ / size_;
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
    int oldDataCount = data_count_;
    if(!table_) {
        return false;
    }
    LinkedList<HashData> *oldTable = table_;
    LinkedList<HashData> *newTable = new(std::nothrow) LinkedList<HashData> [size_ * 2];
    unsigned int oldSize = size_;
    size_ *= 2;
    power_ ++;
    table_ = newTable;
    for(unsigned int i = 0; i < oldSize; i++){
        oldTable[i].resetNodeIndex();
        while(oldTable[i].hasNode()){
            HashData tmpData = oldTable[i].getNode();
            oldTable[i].advanceNode();
            insertNode(tmpData.key_, tmpData.data_);
        }
    }
    delete [] oldTable;
    is_rehashing_ = false;
    return true;
}

template <typename T>
unsigned int HashTable<T>::hash(int key){
    unsigned int hashValue = hashFunction(key);
    return hashValue;
}

template <typename T>
unsigned int HashTable<T>::hashFunction(int key){
    return (key * 0x9E3779B97F4A7C15) >> (64 - power_);
}

template <typename T>
unsigned int HashTable<T>::getSize(){
    if(!table_){
        return 0;
    }
    else{
        return size_;
    }
}

template <typename T>
unsigned int HashTable<T>::getDataCount(){
    return data_count_;
}

template <typename T>
void HashTable<T>::printAll(){
    for(unsigned int i = 0; i < size_; i++){
        table_[i].printAll();
    }
}

template <typename T>
bool HashTable<T>::clear(){
    if(table_){
        size_ = 0;
        power_ = 0;
        data_count_ = 0;
        delete [] table_;
        table_ = nullptr;
        return true;
    } else{
        return false;
    }
}