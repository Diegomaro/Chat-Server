#pragma once

template <typename T>
class LinkedList {
public:
	class Node {
		public:
			T data_;
			Node *next_;
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
	bool clear();
private:
	Node *current_node_;
	Node *head_;
	Node *tail_;
};
#include "linked_list.tpp"