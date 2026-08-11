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
bool HashTable<T>::createTable(unsigned int desiredSize){
    if(table_){
        clear();
    }
    table_ = new LinkedList<HashData> [desiredSize];
    double tmpPower = std::log2(desiredSize);
    if(tmpPower  - static_cast<int>(tmpPower) != 0.f){
        return false;
    }
    power_ = static_cast<int>(tmpPower);
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
    table_[hash(key)].insertTail(hashData);
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
    T *tmpData = getNode(key);
    if(!tmpData){
        return false;
    }
    HashData hashData;
    hashData.key_ = key;
    hashData.data_ = *tmpData;
    if(table_[hash(key)].deleteNode(hashData)){
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
    T *tmpData = getNode(key);
    if(!tmpData){
        return false;
    }
    return true;
}

template <typename T>
T *HashTable<T>::getNode(int key){
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
LinkedList<typename HashTable<T>::HashData> *HashTable<T>::getLinkedList(int key){
    if(!table_){
        return nullptr;
    }
    return &table_[hash(key)];
}

template <typename T>
bool HashTable<T>::hasNodes(){
    if(current_node_ >= size_){
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
        if(current_node_ >= size_){
            return false;
        } else{
            current_node_++;
        }
    }
    return true;
}

template <typename T>
void HashTable<T>::resetNodeIndex(){
    for(unsigned int i = 0; i < size_; i++){
        table_[i].resetNodeIndex();
    }
    current_node_ = 0;
}

template <typename T>
typename HashTable<T>::HashData* HashTable<T>::getNode(){
    return &table_[current_node_].getNode();
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
    unsigned int oldDataCount = data_count_;
    if(!table_) {
        return false;
    }
    LinkedList<HashData> *oldTable = table_;
    LinkedList<HashData> *newTable = new LinkedList<HashData> [size_ * 2];
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
    data_count_ = oldDataCount;
    return true;
}

template <typename T>
unsigned int HashTable<T>::hash(int key){
    unsigned int hashValue = hashFunction(key);
    return hashValue;
}

template <typename T>
unsigned int HashTable<T>::hashFunction(int key){
    return static_cast<int>((key * 0x9E3779B97F4A7C15) >> (64 - power_));
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
void HashTable<T>::clear(){
    if(table_){
        size_ = 0;
        power_ = 0;
        data_count_ = 0;
        delete [] table_;
        table_ = nullptr;
    }
}