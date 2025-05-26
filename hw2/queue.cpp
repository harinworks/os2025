#include <iostream>
#include <cstdlib>
#include "queue.h"


Queue* init(void) {
	auto queue = reinterpret_cast<Queue*>(std::malloc(sizeof(Queue)));
	*queue = { .head = nullptr, .tail = nullptr };

	return queue;
}

void release(Queue* queue) {
	if (queue != nullptr)
		std::free(queue);
}

Node* nalloc(Item item) {
	auto node = reinterpret_cast<Node*>(std::malloc(sizeof(Node)));
	*node = { .item = item, .next = nullptr };

	return node;
}

void nfree(Node* node) {
	if (node != nullptr)
		std::free(node);
}

Node* nclone(Node* node) {
	if (node == nullptr)
		return nullptr;

	auto new_node = nalloc(node->item);

	if (new_node == nullptr)
		return nullptr;

	new_node->next = node->next;

	return new_node;
}

Reply enqueue(Queue* queue, Item item) {
	Reply reply = { false, item };

	if (queue == nullptr)
		return reply;

	auto new_node = nalloc(item);

	if (new_node == nullptr)
		return reply;

	if (queue->head == nullptr) {
		queue->head = new_node;
		queue->tail = new_node;
	} else {
		queue->tail->next = new_node;
		queue->tail = new_node;
	}

	reply.success = true;

	return reply;
}

Reply dequeue(Queue* queue) {
	Reply reply = { false, { 0, nullptr } };

	if (queue == nullptr || queue->head == nullptr)
		return reply;

	auto node = queue->head;

	reply.item = node->item;
	queue->head = node->next;

	if (queue->head == nullptr)
		queue->tail = nullptr;

	nfree(node);
	reply.success = true;

	return reply;
}

Queue* range(Queue* queue, Key start, Key end) {
	if (queue == nullptr)
		return nullptr;

	auto new_queue = init();

	if (new_queue == nullptr)
		return nullptr;

	for (auto node = queue->head; node != nullptr; node = node->next) {
		if (node->item.key >= start && node->item.key <= end) {
			if (!enqueue(new_queue, node->item).success) {
				release(new_queue);
				return nullptr;
			}
		}
	}

	return new_queue;
}
