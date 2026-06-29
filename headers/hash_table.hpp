#pragma once
#include "linked_list.hpp"
#include <string>

#define LOAD_FACTOR 0.7f

template <typename T>
class HashTable{
    public:
        struct HashData{
            int key_;
            T data_;
        };
        HashTable();
        ~HashTable();
        bool createTable(unsigned int desired_size);

        bool insertNode(int key, T data);
        bool deleteNode(int key);
        bool searchNode(int key);
        const T *getNode(int key);

        bool hasNodes();
        bool hasNode();
	    bool advanceNode();
        void resetNodeIndex();
        const HashData* getNode();

        unsigned int getSize();
        unsigned int getDataCount();
        void printAll();
        bool clear();
    private:
        unsigned int hash(int key);
        unsigned int hashFunction(int key);
        bool checkRehash();
        bool rehash();
        bool is_rehashing_;
        LinkedList<HashData> *table_;
        unsigned int size_;
        unsigned int power_;
        unsigned int data_count_;

        int current_node_;
};

#include "hash_table.tpp"