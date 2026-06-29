#include <iostream>
#include "linked_list.hpp"

template <typename T>
LinkedList<T>::Node::Node() {
	next = nullptr;
}
template <typename T>
LinkedList<T>::LinkedList() {
	head = nullptr;
	tail = nullptr;
	curNode = nullptr;
}

template <typename T>
LinkedList<T>::~LinkedList() {
	clear();
}

template <typename T>
bool LinkedList<T>::insertHead(T data) {
	Node *newNode = nullptr;
	newNode = new(std::nothrow) Node;
	if(!newNode){
		return false;
	}
	newNode->data = data;
	newNode->next = head;
	head = newNode;
	curNode = head;
	if(!head->next){
		tail = head;
	}
	return true;
}

template <typename T>
bool LinkedList<T>::insertTail(T data){
	if(!head){
		return insertHead(data);
	}
	Node* newNode = nullptr;
	newNode = new(std::nothrow) Node;
	if(!newNode){
		return false;
	}
	newNode->data = data;
	tail->next = newNode;
	tail = newNode;
    return true;
}

template <typename T>
bool LinkedList<T>::deleteHead(){
	if(!head){
		return false;
	}
	if(head == tail){
		delete head;
		head = nullptr;
		tail = nullptr;
		curNode = nullptr;
		return true;
	}
	Node* temp = head->next;
	if(head == curNode){
		curNode = temp;
	}
	delete head;
	head = temp;
	return true;
}

template <typename T>
bool LinkedList<T>::deleteTail(){
	if(!tail){
		return false;
	}
	if(head == tail){
		delete head;
		head = nullptr;
		tail = nullptr;
		curNode = nullptr;
		return true;
	}

	Node* temp = head;
	while(temp->next->next != nullptr){
        temp = temp->next;
    }
	if(tail == curNode){
		curNode = temp;
	}
	delete tail;
	tail = temp;
	tail->next = nullptr;
	return true;
}

template <typename T>
bool LinkedList<T>::deleteNode(T data){
	if(!head){
		return false;
	}
	if((head==tail) && (head->data == data)){
		delete head;
		head = nullptr;
		tail = nullptr;
		curNode = nullptr;
		return true;
	}
	Node* tmp = head;
	if(head->data == data){
		return deleteHead();
		return true;
	}
	while(tmp->next != nullptr){
		if(tmp->next->data == data){
			Node* anchorNode = tmp->next->next;
			if(curNode == tmp->next){
				curNode = anchorNode;
			}
			delete tmp->next;
			tmp->next = anchorNode;
			if(anchorNode == nullptr){
				tail = tmp;
			}
			return true;
		}
		tmp = tmp->next;
	}
	return false;
}

template <typename T>
bool LinkedList<T>::searchNode(T data){
	if(!head){
		return false;
	}
	Node* index = head;
	while(index != nullptr){
		if(index->data == data){
			return true;
		}
		index = index->next;
	}
	return false;
}

template <typename T>
void LinkedList<T>::resetNodeIndex(){
	curNode = head;
}

template <typename T>
bool LinkedList<T>::hasNode(){
	if(curNode) return true;
	return false;
}

template <typename T>
bool LinkedList<T>::advanceNode(){
	if(!curNode){
		return false;
	}
	curNode = curNode->next;
	if(!curNode){
		return false;
	}
	return true;
}


template <typename T>
T &LinkedList<T>::getNode(){
	Node* tmp = curNode;
	return tmp->data;
}

template <typename T>
bool LinkedList<T>::printAll(){
	if(!head){
		return false;
	}
    Node* index = nullptr;
	index = head;
	std::cout << "lista:" << std::endl;
    while(index != nullptr){
        std::cout << index->data << ", ";
        index = index->next;
    }
	std::cout << std::endl;
	return true;
}

template <typename T>
bool LinkedList<T>::clear(){
	if(!head){
		return false;
	}
	Node* index = head;

	while(index != nullptr){
		index = index->next;
		delete head;
		head = index;
	}
	tail = nullptr;
	curNode = nullptr;
	return true;
}