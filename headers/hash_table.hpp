#pragma once
#include <string>
#include <list>

#define LOAD_FACTOR 0.7f

template <typename T>
class HashTable{
    public:
        struct HashData{
            uint32_t key_;
            T data_;
            bool operator == (const HashData &HASHDATA);
        };
        HashTable();
        ~HashTable();
        bool createTable(uint32_t desired_size);

        bool insertNode(uint32_t key, T data);
        bool deleteNode(uint32_t key);
        bool searchNode(uint32_t key);
        T *getNode(uint32_t key);
        std::list<HashData> *getList(uint32_t key);

        void resetListPtr();
	    bool advanceListPtr();
        std::list<HashData> *getListPtr();

        std::size_t getSize();
        std::size_t getDataCount();
        void clear();
    private:
        unsigned int hash(uint32_t key);
        unsigned int hashFunction(uint32_t key);
        bool checkRehash();
        bool rehash();
        bool is_rehashing_{false};
        std::list<HashData> *table_{nullptr};
        std::size_t size_{0};
        unsigned int power_{0};
        std::size_t data_count_{0};
        std::size_t current_node_{0};
};

#include "hash_table.tpp"