#pragma once

template <typename T>
class LinkedList {
public:
	class Node {
		public:
			T data_;
			Node *next_{nullptr};
			Node();
	};

	LinkedList();
	~LinkedList();
	void insertHead(const T& data);
	void insertTail(const T& data);
	void deleteHead();
	void deleteTail();
	bool deleteNode(const T& data);
	bool searchNode(const T& data);
	void resetNodeIndex();
	bool hasNode();
	bool advanceNode();
	T &getNode();
	T &getHead();
	bool isEmpty();
	uint32_t getSize();
	void clear();
private:
	Node *current_node_{nullptr};
	Node *head_{nullptr};
	Node *tail_{nullptr};
	uint32_t size_{0};
};
#include "linked_list.tpp"