#if !defined(LIBROBOCOL_FIXEDBUF_H)
#define LIBROBOCOL_FIXEDBUF_H

#include <memory>

namespace librobocol
{
        
    class FixedBufItr
    {
    public:

        using iterator_category = std::random_access_iterator_tag;
        using value_type = char;
        using difference_type = std::ptrdiff_t;
        using pointer = char*;
        using reference = char&;

    public:

        FixedBufItr(char* ptr = nullptr) { this->ptr = ptr;}
        FixedBufItr(const FixedBufItr& rawIterator) = default;
        FixedBufItr(char* ptr, char* end) { this->ptr = ptr; this->end = end; }
        ~FixedBufItr(){}

        FixedBufItr&                  operator=(const FixedBufItr& rawIterator) = default;
        FixedBufItr&                  operator=(char* ptr){ptr = ptr;return (*this);}

        operator                      bool()const
        {
            if(ptr)
                return true;
            else
                return false;
        }

        bool                                        operator==(const FixedBufItr& rawIterator)const{return (ptr == rawIterator.getConstPtr());}
        bool                                        operator!=(const FixedBufItr& rawIterator)const{return (ptr != rawIterator.getConstPtr());}

        FixedBufItr&                  operator+=(const difference_type& movement){ptr += movement;return (*this);}
        FixedBufItr&                  operator-=(const difference_type& movement){ptr -= movement;return (*this);}
        FixedBufItr&                  operator++(){++ptr;return (*this);}
        FixedBufItr&                  operator--(){--ptr;return (*this);}
        FixedBufItr                   operator++(int){auto temp(*this);++ptr;return temp;}
        FixedBufItr                   operator--(int){auto temp(*this);--ptr;return temp;}
        FixedBufItr                   operator+(const difference_type& movement){auto oldPtr = ptr;ptr+=movement;auto temp(*this);ptr = oldPtr;return temp;}
        FixedBufItr                   operator-(const difference_type& movement){auto oldPtr = ptr;ptr-=movement;auto temp(*this);ptr = oldPtr;return temp;}

        difference_type                             operator-(const FixedBufItr& rawIterator){return std::distance(rawIterator.getPtr(),this->getPtr());}

        char&                                 operator*()
        {
            if (ptr >= end)
            {
                printf("VECTOR OVERRUN %p %p\n", ptr, end);
            }
            return *ptr;
        }
        //const char&                           operator*()const{return *ptr;}
        char*                                 operator->(){return ptr;}

        char*                                 getPtr()const{return ptr;}
        const char*                           getConstPtr()const{return ptr;}

    protected:

        char* ptr;

    #ifndef NDEBUG
        char* end;
    #endif
    };


    struct FixedBuf
    {
        size_t len = 0;
        std::shared_ptr<char[]> buf;

        FixedBuf(size_t len)
        {
            // assert((len & (len - 1)) == 0);
            // assert(len > 0);

            printf("Making a fixedbuf of size %u\n", len);

            this->len = len;
            //buf = std::make_unique_for_overwrite<char[]>(len);
            buf = std::make_shared<char[]>(len);
        }

        FixedBuf(FixedBuf &&other)
        {
            if (len != 0 || buf != nullptr) { printf("MOVING INTO A CONSTRUCTED OBJECT\n"); }

            len = other.len;
            buf = std::move(other.buf);

            other.len = 0;
        }
        FixedBuf &operator=(FixedBuf &&other)
        {
            if (len != 0 || buf != nullptr) { printf("MOVING INTO A CONSTRUCTED OBJECT\n"); }
            len = other.len;
            buf = std::move(other.buf);

            other.len = 0;

            return *this;
        }

        /*FixedBuf(nullptr_t)
        {
            len = 0;
            buf = nullptr;
        }*/

        FixedBuf()
        {
            len = 0;
            buf = nullptr;
        }

        ~FixedBuf() 
        {
            if (len > 0 && buf != nullptr) {
            printf("FREEING A FIXEDBUF\n");}
        
        }

        // todo: remove once our compiler gains the ability to put noncopyable types in vectors??
        FixedBuf(const FixedBuf &other)
        {
            printf("Making a fixedbuf of size %u", len);
            
            this->len = other.len;
            buf = other.buf;

            /*if (other.isValid())
            {
                memcpy(buf.get(), other.buf.get(), other.len);
            }*/
        }
        FixedBuf &operator=(const FixedBuf &other)
        {
            printf("Making a fixedbuf of size %u", len);

            this->len = other.len;
            this->buf = other.buf;
            /*buf = std::make_unique<char[]>(len);

            if (other.isValid())
            {
                memcpy(buf.get(), other.buf.get(), other.len);
            }*/
            return *this;
        }

        /*FixedBuf() = default;

        FixedBuf(const FixedBuf&) = delete;
        FixedBuf& operator=(const FixedBuf&) = delete;
        FixedBuf& operator=(FixedBuf&& other)
        {
            len = other.len;
            buf = std::move(other.buf);
            return *this;
        }*/

        bool isValid() const noexcept
        {
            return len != 0 && buf.get() != nullptr;
        }

        FixedBufItr begin()
        {
            #ifndef NDEBUG
            return FixedBufItr(buf.get(), buf.get() + len);
            #else
            return FixedBufItr(buf.get());
            #endif
        }

        FixedBufItr end()
        {
            #ifndef NDEBUG
            return FixedBufItr(buf.get() + len, buf.get() + len);
            #else
            return FixedBufItr(buf.get() + len);
            #endif
        }

        char *data()
        {
            return buf.get();
        }

        size_t size()
        {
            return len;
        }
    };
}

#endif // if !defined(LIBROBOCOL_FIXEDBUF_H)
