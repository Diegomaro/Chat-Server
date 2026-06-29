#include "cmath"
#include "hash_table.hpp"

template <typename T>
HashTable<T>::HashTable(){
    is_rehashing = false;
    table = nullptr;
    dataCount = 0;
    size = 0;
    power = 0;
    curNode = 0;
}

template <typename T>
HashTable<T>::~HashTable(){
    clear();
}

template <typename T>
bool HashTable<T>::createTable(unsigned int desiredSize){
    if(table || !desiredSize){
        return false;
    }
    table = new(std::nothrow) LinkedList<HashData> [desiredSize];
    if(!table) {
        return false;
    }
    double tmpPower = std::log2(desiredSize);
    if(tmpPower  - (int)tmpPower != 0.f){
        return false;
    }
    power = (int) tmpPower;
    size = desiredSize;
    return true;
}

template <typename T>
bool HashTable<T>::insertNode(int key, T data){
    if(!table){
        return false;
    }
    HashData hashData;
    hashData.key = key;
    hashData.data = data;
    if(!table[hash(key)].insertTail(hashData)){
        return false;
    }
    if(is_rehashing){
        return true;
    }
    dataCount++;
    if(!checkRehash()){
        return false;
    }
    return true;
}

template <typename T>
bool HashTable<T>::deleteNode(int key){
    if(!table){
        return false;
    }
    if(table[hash(key)].deleteNode(key)){
        dataCount--;
        return true;
    }
    return false;
}

template <typename T>
bool HashTable<T>::searchNode(int key){
    if(!table){
        return false;
    }
    if(table[hash(key)].searchNode(key)){
        return true;
    }
    return false;
}

template <typename T>
const T *HashTable<T>::getNode(int key){
    if(!table){
        return nullptr;
    }
    table[hash(key)].resetNodeIndex();
    while(table[hash(key)].hasNode()){
        HashData tmpData = table[hash(key)].getNode();
        if(tmpData.key == key){
            return &table[hash(key)].getNode().data;
        }
        table[hash(key)].advanceNode();
    }
    return nullptr;
}

template <typename T>
bool HashTable<T>::hasNodes(){
    if(curNode + 1 >= size){
        return false;
    }
    return true;
}

template <typename T>
bool HashTable<T>::hasNode(){
    if(!table[curNode].hasNode()){
        return false;
    }
    return true;
}

template <typename T>
bool HashTable<T>::advanceNode(){
    if(!table[curNode].advanceNode()){
        if(curNode + 1 >= size){
            return false;
        } else{
            curNode++;
        }
    } else{
        table[curNode].resetNodeIndex();
    }
    return true;
}

template <typename T>
void HashTable<T>::resetNodeIndex(){
    table[0].resetNodeIndex();
    curNode = 0;
}

template <typename T>
const typename HashTable<T>::HashData* HashTable<T>::getNode(){
    return &table[curNode].getNode();
}

template <typename T>
bool HashTable<T>::checkRehash(){
    if(!size){
        return false;
    }
    float loadFactor = (float) dataCount / size;
    if(loadFactor >= LOAD_FACTOR){
        if(!rehash()){
            return false;
        }
    }
    return true;
}


template <typename T>
bool HashTable<T>::rehash(){
    is_rehashing = true;
    int oldDataCount = dataCount;
    if(!table) {
        return false;
    }
    LinkedList<HashData> *oldTable = table;
    LinkedList<HashData> *newTable = new(std::nothrow) LinkedList<HashData> [size * 2];
    unsigned int oldSize = size;
    size *= 2;
    power ++;
    table = newTable;
    for(unsigned int i = 0; i < oldSize; i++){
        oldTable[i].resetNodeIndex();
        while(oldTable[i].hasNode()){
            HashData tmpData = oldTable[i].getNode();
            oldTable[i].advanceNode();
            insertNode(tmpData.key, tmpData.data);
        }
    }
    delete [] oldTable;
    is_rehashing = false;
    return true;
}

template <typename T>
unsigned int HashTable<T>::hash(int key){
    unsigned int hashValue = hashFunction(key);
    return hashValue;
}

template <typename T>
unsigned int HashTable<T>::hashFunction(int key){
    return (key * 0x9E3779B97F4A7C15) >> (64 - power);
}

template <typename T>
unsigned int HashTable<T>::getSize(){
    if(!table){
        return 0;
    }
    else{
        return size;
    }
}

/*template <typename T>
LinkedList<typename HashTable<T>::HashData> * HashTable<T>::getTable(){
    return &table;
}*/

template <typename T>
unsigned int HashTable<T>::getDataCount(){
    return dataCount;
}

template <typename T>
void HashTable<T>::printAll(){
    for(unsigned int i = 0; i < size; i++){
        table[i].printAll();
    }
}

template <typename T>
bool HashTable<T>::clear(){
    if(table){
        size = 0;
        power = 0;
        dataCount = 0;
        delete [] table;
        table = nullptr;
        return true;
    } else{
        return false;
    }
}