/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2020-2025 Gregg E. Berman
 *  
 *  https://github.com/HomeSpan/HomeSpan
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 ********************************************************************************/
 
#pragma once

#ifndef HS_MALLOC

#if defined(BOARD_HAS_PSRAM)
static inline void *hs_malloc(size_t size){                         // prefer PSRAM, fall back to internal heap
  void *p=ps_malloc(size);
  return(p!=NULL?p:malloc(size));
}
static inline void *hs_calloc(size_t n, size_t size){
  void *p=ps_calloc(n,size);
  return(p!=NULL?p:calloc(n,size));
}
static inline void *hs_realloc(void *ptr, size_t size){
  if(size==0){                                                     // realloc to zero frees the block: handle here to avoid a double free on fallback
    free(ptr);
    return(NULL);
  }
  void *p=ps_realloc(ptr,size);
  return(p!=NULL?p:realloc(ptr,size));                             // on failure the original block is still valid
}
#define HS_MALLOC hs_malloc
#define HS_CALLOC hs_calloc
#define HS_REALLOC hs_realloc
#define ps_new(X) new(HS_MALLOC(sizeof(X)))X
#else
#define HS_MALLOC malloc
#define HS_CALLOC calloc
#define HS_REALLOC realloc
#define ps_new(X) new X
#endif

template <class T>
struct Mallocator {
  typedef T value_type;
  Mallocator() = default;
  template <class U> constexpr Mallocator(const Mallocator<U>&) {}
  [[nodiscard]] T* allocate(std::size_t n) {
    auto p = static_cast<T*>(HS_MALLOC(n*sizeof(T)));
    if(p==NULL){
      Serial.printf("\n\n*** FATAL ERROR: Requested allocation of %d bytes failed.  Program Halting.\n\n",n*sizeof(T));
      while(1);
    }
    return p;
  }
  void deallocate(T* p, std::size_t) noexcept { std::free(p); }
};
template <class T, class U>
bool operator==(const Mallocator<T>&, const Mallocator<U>&) { return true; }
template <class T, class U>
bool operator!=(const Mallocator<T>&, const Mallocator<U>&) { return false; }

#endif
