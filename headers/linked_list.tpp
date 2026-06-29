#include <iostream>
#include "linked_list.hpp"

template <typename T>
LinkedList<T>::Node::Node() {
	next_ = nullptr;
}
template <typename T>
LinkedList<T>::LinkedList() {
	head_ = nullptr;
	tail_ = nullptr;
	current_node_ = nullptr;
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
	newNode->data_ = data;
	newNode->next_ = head_;
	head_ = newNode;
	current_node_ = head_;
	if(!head_->next_){
		tail_ = head_;
	}
	return true;
}

template <typename T>
bool LinkedList<T>::insertTail(T data){
	if(!head_){
		return insertHead(data);
	}
	Node* newNode = nullptr;
	newNode = new(std::nothrow) Node;
	if(!newNode){
		return false;
	}
	newNode->data_ = data;
	tail_->next_ = newNode;
	tail_ = newNode;
    return true;
}

template <typename T>
bool LinkedList<T>::deleteHead(){
	if(!head_){
		return false;
	}
	if(head_ == tail_){
		delete head_;
		head_ = nullptr;
		tail_ = nullptr;
		current_node_ = nullptr;
		return true;
	}
	Node* temp = head_->next_;
	if(head_ == current_node_){
		current_node_ = temp;
	}
	delete head_;
	head_ = temp;
	return true;
}

template <typename T>
bool LinkedList<T>::deleteTail(){
	if(!tail_){
		return false;
	}
	if(head_ == tail_){
		delete head_;
		head_ = nullptr;
		tail_ = nullptr;
		current_node_ = nullptr;
		return true;
	}

	Node* temp = head_;
	while(temp->next->next != nullptr){
        temp = temp->next;
    }
	if(tail_ == current_node_){
		current_node_ = temp;
	}
	delete tail_;
	tail_ = temp;
	tail_->next = nullptr;
	return true;
}

template <typename T>
bool LinkedList<T>::deleteNode(T data){
	if(!head_){
		return false;
	}
	if((head_==tail_) && (head_->data_ == data)){
		delete head_;
		head_ = nullptr;
		tail_ = nullptr;
		current_node_ = nullptr;
		return true;
	}
	Node* tmp = head_;
	if(head_->data_ == data){
		return deleteHead();
		return true;
	}
	while(tmp->next != nullptr){
		if(tmp->next->data == data){
			Node* anchorNode = tmp->next->next;
			if(current_node_ == tmp->next){
				current_node_ = anchorNode;
			}
			delete tmp->next;
			tmp->next = anchorNode;
			if(anchorNode == nullptr){
				tail_ = tmp;
			}
			return true;
		}
		tmp = tmp->next;
	}
	return false;
}

template <typename T>
bool LinkedList<T>::searchNode(T data){
	if(!head_){
		return false;
	}
	Node* index = head_;
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
	current_node_ = head_;
}

template <typename T>
bool LinkedList<T>::hasNode(){
	if(current_node_) return true;
	return false;
}

template <typename T>
bool LinkedList<T>::advanceNode(){
	if(!current_node_){
		return false;
	}
	current_node_ = current_node_->next_;
	if(!current_node_){
		return false;
	}
	return true;
}


template <typename T>
T &LinkedList<T>::getNode(){
	Node* tmp = current_node_;
	return tmp->data_;
}

template <typename T>
bool LinkedList<T>::printAll(){
	if(!head_){
		return false;
	}
    Node* index = nullptr;
	index = head_;
	std::cout << "lista:" << std::endl;
    while(index != nullptr){
        std::cout << index->data_ << ", ";
        index = index->next_;
    }
	std::cout << std::endl;
	return true;
}

template <typename T>
bool LinkedList<T>::clear(){
	if(!head_){
		return false;
	}
	Node* index = head_;

	while(index != nullptr){
		index = index->next_;
		delete head_;
		head_ = index;
	}
	tail_ = nullptr;
	current_node_ = nullptr;
	return true;
}