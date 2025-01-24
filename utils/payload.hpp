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
#else
    static void freeBlob(char* b, size_t sz){
        free(b);
    }
    
    static void freeTask(ExcType* ptr){
        return;
    }
    
    std::tuple<char*, size_t, bool> serialize(){
        return {this->content, MessageSize, false};
    }
    
    bool deserialize(char* buffer, size_t sz){
        this->content = buffer;
        return false;
    }
    
    static ExcType* alloc(char* buf, size_t sz){
        return new ExcType;
    }
#endif
};

#if 0
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

    static void freeBlob(char* b, size_t sz){
#ifndef REUSE_PAYLOAD
        delete reinterpret_cast<ExcType*>(b);
#endif
    }
    
    static void freeTask(ExcType* ptr){
        return;
    }
    
    std::tuple<char*, size_t, bool> serialize(){
        return {(char*)this, sizeof(ExcType), false};
    }
    
    bool deserialize(char* buffer, size_t sz){
        return false;
    }
    
    static ExcType* alloc(char* buf, size_t sz){
        return reinterpret_cast<ExcType*>(buf);
    }

};

#endif

#endif //PAYLOAD_HPP