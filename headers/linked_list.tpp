#include <iostream>
#include "linked_list.hpp"

template <typename T>
LinkedList<T>::Node::Node() {}

template <typename T>
LinkedList<T>::LinkedList() {}

template <typename T>
LinkedList<T>::~LinkedList() {
	clear();
}

template <typename T>
void LinkedList<T>::insertHead(const T& data) {
	Node *newNode = new Node;
	newNode->data_ = data;
	newNode->next_ = head_;
	head_ = newNode;
	current_node_ = head_;
	if(!head_->next_){
		tail_ = head_;
	}
	size_++;
}

template <typename T>
void LinkedList<T>::insertTail(const T& data){
	if(!head_){
		insertHead(data);
		return;
	}
	Node* newNode = new Node;
	newNode->data_ = data;
	tail_->next_ = newNode;
	tail_ = newNode;
	size_++;
}

template <typename T>
void LinkedList<T>::deleteHead(){
	if(!head_){
		return;
	}
	if(head_ == tail_){
		delete head_;
		head_ = nullptr;
		tail_ = nullptr;
		current_node_ = nullptr;
		size_--;
		return;
	}
	Node* temp = head_->next_;
	if(head_ == current_node_){
		current_node_ = temp;
	}
	delete head_;
	head_ = temp;
	size_--;
}

template <typename T>
void LinkedList<T>::deleteTail(){
	if(!tail_){
		return;
	}
	if(head_ == tail_){
		delete head_;
		head_ = nullptr;
		tail_ = nullptr;
		current_node_ = nullptr;
		size_--;
		return;
	}

	Node* temp = head_;
	while(temp->next_->next_ != nullptr){
        temp = temp->next_;
    }
	if(tail_ == current_node_){
		current_node_ = temp;
	}
	delete tail_;
	tail_ = temp;
	tail_->next_ = nullptr;
	size_--;
}

template <typename T>
bool LinkedList<T>::deleteNode(const T& data){
	if(!head_){
		return false;
	}
	if(head_->data_ == data){
		deleteHead();
		return true;
	}
	Node* tmp = head_;
	while(tmp->next_ != nullptr){
		if(tmp->next_->data_ == data){
			Node* anchorNode = tmp->next_->next_;
			if(current_node_ == tmp->next_){
				current_node_ = anchorNode;
			}
			delete tmp->next_;
			tmp->next_ = anchorNode;
			if(anchorNode == nullptr){
				tail_ = tmp;
			}
			size_--;
			return true;
		}
		tmp = tmp->next_;
	}
	return false;
}

template <typename T>
bool LinkedList<T>::searchNode(const T& data){
	if(!head_){
		return false;
	}
	Node* index = head_;
	while(index != nullptr){
		if(index->data_ == data){
			return true;
		}
		index = index->next_;
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
	return current_node_ != nullptr;
}

template <typename T>
T &LinkedList<T>::getNode(){
	Node* tmp = current_node_;
	return tmp->data_;
}

template <typename T>
T &LinkedList<T>::getHead(){
	return head_->data_;
}

template <typename T>
bool LinkedList<T>::isEmpty(){
	return head_ == nullptr;
}

template <typename T>
uint32_t LinkedList<T>::getSize(){
	return size_;
}

template <typename T>
void LinkedList<T>::clear(){
	if(!head_){
		return;
	}
	Node* index = head_;

	while(index != nullptr){
		index = index->next_;
		delete head_;
		head_ = index;
	}
	tail_ = nullptr;
	current_node_ = nullptr;
	size_ = 0;
	return;
}