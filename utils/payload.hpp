#ifndef PAYLOAD_HPP
#define PAYLOAD_HPP

#include <stdlib.h>

size_t MessageSize = 0;

#ifndef BASE_TYPE_PAYLOAD

struct ExcType {
    char* content = nullptr;
	ExcType() { }

    ExcType(size_t s){
        content = (char*)malloc(MessageSize);
#ifdef DEBUG
        content[0] = 'C';
        content[MessageSize-1] = 'O';
#endif
    }

    ~ExcType(){
        if (content) free(content);
    }
	
	
#if !defined(MANUAL_SERIALIZATION)
    template <class Archive>
    void save( Archive & ar ) const {
        if (content) ar(cereal::binary_data(content, MessageSize));
    }
      
    template <class Archive>
    void load( Archive & ar ) {
        content = (char*)malloc(MessageSize);
        ar(cereal::binary_data(content, MessageSize));
    }
#endif
		
};

#ifdef MANUAL_SERIALIZATION
template<typename Buffer>
bool serialize(Buffer&b, ExcType* input){
	b = {input->content, MessageSize};
	return false;
}

template<typename T>
void serializefreetask(T *o, ExcType* input) {
#ifndef REUSE_PAYLOAD
    delete input;
#endif
}

template<typename Buffer>
bool deserialize(const Buffer&b, ExcType* p){
	p->content = b.first;
	return false;
}
#endif // MANUAL_SERIALIZATION

#else // BASE_TYPE_PAYLOAD

struct ExcType {
    int p;
    ExcType(int p) : p(p) {};
};

template<typename Buffer>
bool serialize(Buffer&b, ExcType* input){
	b = {(char*)input, sizeof(ExcType)};
	return false;
}

template<typename T>
void serializefreetask(T *o, ExcType* input) {
#ifndef REUSE_PAYLOAD
	delete input;
#endif
}

template<typename Buffer>
void deserializealloctask(const Buffer& b, ExcType*& p) {
        p = reinterpret_cast<ExcType*>(b.first);
};

template<typename Buffer>
bool deserialize(const Buffer&b, ExcType* p){
	return false;
}

#endif

#endif //PAYLOAD_HPP