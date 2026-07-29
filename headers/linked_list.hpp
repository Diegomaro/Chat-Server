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
	bool insertHead(T data);
	bool insertTail(T data);
	bool deleteHead();
	bool deleteTail();
	bool deleteNode(T data);
	bool searchNode(T data);
	void resetNodeIndex();
	bool hasNode();
	bool advanceNode();
	T &getNode();
	T &getHead();
	bool printAll();
	bool isEmpty();
	uint32_t getSize();
	bool clear();
private:
	Node *current_node_{nullptr};
	Node *head_{nullptr};
	Node *tail_{nullptr};
	uint32_t size_{0};
};
#include "linked_list.tpp"