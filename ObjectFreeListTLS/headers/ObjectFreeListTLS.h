#pragma once

#include "objectFreeList\headers\ObjectFreeList.h"
#pragma comment(lib, "lib/objectFreeList/ObjectFreeList")

#include "common.h"

#if defined(OBJECT_FREE_LIST_TLS_SAFE)
	#define allocObject() _allocObject(__FILEW__, __LINE__)
	#define freeObject(ptr) _freeObject(ptr, __FILEW__, __LINE__)
#else
	#define allocObject() _allocObject()
	#define freeObject(ptr) _freeObject(ptr)
#endif

template <typename T>
struct stAllocChunk;

template <typename T>
struct stAllocTlsNode;

template <typename T>
class CObjectFreeListTLS{

public:

	CObjectFreeListTLS(bool runConstructor, bool runDestructor);

	T*	 _allocObject(
		#if defined(OBJECT_FREE_LIST_TLS_SAFE)
			const wchar_t*, int
		#endif
	);
	void _freeObject (T* object
		#if defined(OBJECT_FREE_LIST_TLS_SAFE)
			,const wchar_t*, int
		#endif
	);

	unsigned int getCapacity();
	unsigned int getUsedCount();
	
private:
	
	// �� �����忡�� Ȱ���� ûũ�� ����ִ� tls�� index
	unsigned int _allocChunkTlsIdx;

	// ��� �����尡 ���������� �����ϴ� free list �Դϴ�.
	// �̰����� T type�� node�� ū ����� ����ɴϴ�.
	CObjectFreeList<stAllocChunk<T>>* _centerFreeList;
			
	// T type�� ���� ������ ȣ�� ����
	bool _runConstructor;

	// T type�� ���� �Ҹ��� ȣ�� ����
	bool _runDestructor;

};

template <typename T>
CObjectFreeListTLS<T>::CObjectFreeListTLS(bool runConstructor, bool runDestructor){

	
	///////////////////////////////////////////////////////////////////////
	// �߾� ó���� freeList ����
	_centerFreeList = new CObjectFreeList<stAllocChunk<T>>(false, true);
	///////////////////////////////////////////////////////////////////////
	
	///////////////////////////////////////////////////////////////////////
	// tls �Ҵ� ����
	_allocChunkTlsIdx = TlsAlloc();
	if(_allocChunkTlsIdx == TLS_OUT_OF_INDEXES){
		CDump::crash();
	}
	///////////////////////////////////////////////////////////////////////

	_runConstructor = runConstructor;
	_runDestructor	= runDestructor;

}

template <typename T>
typename T* CObjectFreeListTLS<T>::_allocObject(
	#if defined(OBJECT_FREE_LIST_TLS_SAFE)
		const wchar_t* fileName,
		int line
	#endif
){

	///////////////////////////////////////////////////////////////////////
	// ûũ ȹ��
	stAllocChunk<T>* chunk = (stAllocChunk<T>*)TlsGetValue(_allocChunkTlsIdx);
	if(chunk == nullptr){
		chunk = _centerFreeList->allocObject();
		TlsSetValue(_allocChunkTlsIdx, chunk);
	}
	///////////////////////////////////////////////////////////////////////
	
	///////////////////////////////////////////////////////////////////////
	// ûũ���� ��� ȹ�� & �ʱ�ȭ
	stAllocTlsNode<T>* allocNode = chunk->_allocNode;
	#if defined(OBJECT_FREE_LIST_TLS_SAFE)
		allocNode->_allocated = true;
		allocNode->_allocSourceFileName = fileName;
		allocNode->_allocLine = line;
	#endif
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// ��忡�� T type ������ ȹ��
	T* allocData = &allocNode->_data;
	if(_runConstructor == true){
		new (allocData) T();
	}
	///////////////////////////////////////////////////////////////////////
	
	///////////////////////////////////////////////////////////////////////
	// ���� ��� �ٶ󺸱�
	chunk->_allocNode += 1;
	///////////////////////////////////////////////////////////////////////
	
	///////////////////////////////////////////////////////////////////////
	// ûũ�� ��� ����ߴٸ� ���� �Ҵ�ޱ�
	if(chunk->_allocNode == chunk->_nodeEnd){
		TlsSetValue(_allocChunkTlsIdx, _centerFreeList->allocObject());
	}
	///////////////////////////////////////////////////////////////////////

	return allocData;

}

template <typename T>
void CObjectFreeListTLS<T>::_freeObject(T* object
	#if defined(OBJECT_FREE_LIST_TLS_SAFE)
		,const wchar_t* fileName,
		int line
	#endif
){
	
	///////////////////////////////////////////////////////////////////////
	// �Ҵ��ߴ� ��� ȹ��
	stAllocTlsNode<T>* allocatedNode = (stAllocTlsNode<T>*)((unsigned __int64)object + objectFreeListTLS::_dataToNodePtr);
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// object ���� ��� ���� üũ
	#if defined(OBJECT_FREE_LIST_TLS_SAFE)
		if(allocatedNode->_underflowCheck != objectFreeListTLS::UNDERFLOW_CHECK_VALUE){
			// underflow
			CDump::crash();
		}
		if(allocatedNode->_overflowCheck != objectFreeListTLS::OVERFLOW_CHECK_VALUE){
			// overflow
			CDump::crash();
		}
		if(allocatedNode->_allocated == false){
			// �Ҵ���� ���� ��� ���� �õ�
			CDump::crash();
		}
	#endif
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// ���� ��û�� �ҽ� ���� & ���� ����
	#if defined(OBJECT_FREE_LIST_TLS_SAFE)
		allocatedNode->_freeSourceFileName = fileName;
		allocatedNode->_freeLine = line;
	#endif
	///////////////////////////////////////////////////////////////////////

	
	///////////////////////////////////////////////////////////////////////
	// �Ҹ��� ȣ��
	if(_runDestructor == true){
		object->~T();
	}
	///////////////////////////////////////////////////////////////////////
	
	///////////////////////////////////////////////////////////////////////
	// ��忡�� ûũ ȹ��
	stAllocChunk<T>* chunk = allocatedNode->_afflicatedChunk;
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// ûũ�� ��� ��Ұ� ��� �Ϸ�(�Ҵ� �� ��ȯ)�Ǿ��ٸ� ûũ�� ��ȯ
	chunk->_leftFreeCnt -= 1;
	if(chunk->_leftFreeCnt == 0){
		_centerFreeList->freeObject(chunk);
	}
	///////////////////////////////////////////////////////////////////////
}

template <typename T>
unsigned int CObjectFreeListTLS<T>::getUsedCount(){
	
	return _centerFreeList->getUsedCount();

}

template <typename T>
unsigned int CObjectFreeListTLS<T>::getCapacity(){
	
	return _centerFreeList->getCapacity();

}

template <typename T>
struct stAllocTlsNode{
	
	#if defined(OBJECT_FREE_LIST_TLS_SAFE)
		unsigned __int64 _underflowCheck;
	#endif

	T _data;

	#if defined(OBJECT_FREE_LIST_TLS_SAFE)
		unsigned __int64 _overflowCheck;

		const wchar_t* _allocSourceFileName;
		const wchar_t*  _freeSourceFileName;

		int _allocLine;
		int  _freeLine;

		bool _allocated;

	#endif

	// ��尡 �ҼӵǾ��ִ� ûũ
	stAllocChunk<T>* _afflicatedChunk;

	stAllocTlsNode(stAllocChunk<T>* afflicatedChunk = nullptr);
};

template <typename T>
stAllocTlsNode<T>::stAllocTlsNode(stAllocChunk<T>* afficatedChunk)
{
	#if defined(OBJECT_FREE_LIST_TLS_SAFE)
		_underflowCheck = objectFreeListTLS::UNDERFLOW_CHECK_VALUE;
		_overflowCheck  = objectFreeListTLS::OVERFLOW_CHECK_VALUE;

		_allocSourceFileName = nullptr;
		_freeSourceFileName = nullptr;

		_allocLine = 0;
		_freeLine = 0;
	
		_allocated = false;
	#endif

	_afflicatedChunk = afficatedChunk;
}


template <typename T>
struct stAllocChunk{
		
public:

	int _leftFreeCnt;
	stAllocTlsNode<T>* _allocNode;

	stAllocTlsNode<T> _nodes[objectFreeListTLS::_chunkSize];	
	stAllocTlsNode<T>* _nodeEnd;

	int _nodeNum;

	stAllocChunk();
	~stAllocChunk();

};

template <typename T>
stAllocChunk<T>::stAllocChunk(){
		
	_nodeNum = objectFreeListTLS::_chunkSize;

	_allocNode = _nodes;
	_nodeEnd   = _nodes + _nodeNum;

	_leftFreeCnt = _nodeNum;

	for(int nodeCnt = 0; nodeCnt < _nodeNum; ++nodeCnt){
		new (&_nodes[nodeCnt]) stAllocTlsNode<T>(this);
	}
}

template <typename T>
stAllocChunk<T>::~stAllocChunk(){
	_allocNode = _nodes;
	_leftFreeCnt = _nodeNum;
}