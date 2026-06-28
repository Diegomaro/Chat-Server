#pragma once
#include "linked_list.hpp"
#include <string>

#define LOAD_FACTOR 0.7f

template <typename T>
class HashTable{
    public:
        HashTable();
        ~HashTable();
        bool createTable(unsigned int desired_size);
        bool insertNode(int key, T data);
        bool deleteNode(int key);
        bool searchNode(int key);
        const T *getNode(int key);
        unsigned int getSize();
        //LinkedList<typename HashTable<T>::HashData> *getTable();
        unsigned int getDataCount();
        void printAll();
        bool clear();
        struct HashData{
            int key;
            T data;
        };
    private:
        unsigned int hash(int key);
        unsigned int hashFunction(int key);
        bool checkRehash();
        bool rehash();
        bool is_rehashing;
        LinkedList<HashData> *table;
        unsigned int size;
        unsigned int power;
        unsigned int dataCount;
};

#include "../src/server/hash_table.tpp"