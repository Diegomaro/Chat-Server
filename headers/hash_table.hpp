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
            bool operator == (const HashData &HASHDATA);
        };
        HashTable();
        ~HashTable();
        bool createTable(unsigned int desired_size);

        bool insertNode(int key, T data);
        bool deleteNode(int key);
        bool searchNode(int key);
        T *getNode(int key);

        bool hasNodes();
        bool hasNode();
	    bool advanceNode();
        void resetNodeIndex();
        HashData* getNode();

        unsigned int getSize();
        unsigned int getDataCount();
        void clear();
    private:
        unsigned int hash(int key);
        unsigned int hashFunction(int key);
        bool checkRehash();
        bool rehash();
        bool is_rehashing_{false};
        LinkedList<HashData> *table_{nullptr};
        unsigned int size_{0};
        unsigned int power_{0};
        unsigned int data_count_{0};
        int current_node_{0};
};

#include "hash_table.tpp"