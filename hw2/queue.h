#ifndef _QUEUE_H // header guard
#define _QUEUE_H 

// ==========이 파일은 수정 불가==========
// 참고: 일반적으로 queue_init()과 같이 prefix를 붙여 소속(?)을 명확히 함
// 이 과제에서는 편의를 위해 짧은 이름을 사용

#include "qtype.h"

// 큐 초기화, 해제
Queue* init(void);
void release(Queue* queue);


// ==========concurrent operations==========

// 노드 생성&초기화, 해제, 복제
Node* nalloc(Item item);
void nfree(Node* node);
Node* nclone(Node* node);
 
// (key, item)을 키값에 따라 적절한 위치에 추가
// Reply: 문제가 없으면 success=true, 아니면 success=false
Reply enqueue(Queue* queue, Item item);

//  첫 번째 노드(키값이 가장 큰 노드)를 추출(큐에서 삭제하고 아이템을 리턴)
// Reply: 큐가 비어있으면 success=false
//        아니면          success=true,  item=첫 번째 노드의 아이템
Reply dequeue(Queue* queue);

// start <= key && key <= end 인 노드들을 찾고,
// 해당 노드들을 <복제>해서 새로운 큐에 삽입한 후 포인터를 리턴
Queue* range(Queue* queue, Key start, Key end);

#endif
