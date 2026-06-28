#pragma once

template <typename T>
class LinkedList {
public:
	class Node {
		public:
			T data;
			Node *next;
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
	bool printAll();
	bool clear();
private:
	Node *curNode;
	Node *head;
	Node *tail;
};
#include "linked_list.tpp"