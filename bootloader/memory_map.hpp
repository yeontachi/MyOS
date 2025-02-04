#pragma once

#include <stdint.h> // 고정 크기의 정수 타입을 제공하는 헤더 파일

struct MemoryMap{// UEFI의 메모리 맵을 저장
    unsigned long long buffer_size;     //버퍼 크기
    void* buffer;                       //메모리 맵을 저장하는 버퍼
    unsigned long long map_size;        //메모리 맵 전체 크기(바이트 단위)
    unsigned long long map_key;         //메모리 맵을 사용할 때 필요한 키 값
    unsigned long long descriptor_size; //각 메모리 디스크립터의 크기
    uint32_t descriptor_version;        //메모리 디스크립터 버전
};

struct MemoryDescriptor{// 각 메모리 블록(디스크립터)의 정보를 담는 구조체
    uint32_t type;              //메모리 유형(RAM, ROM, 예약된 영역...)
    uintptr_t physical_start;   //물리 메모리 주소의 시작 위치
    uintptr_t virtual_start;    //가상 메모리 주소의 시작 위치(UEFI 런타임에서 사용됨)
    uint64_t number_of_pages;   //해당 영역이 차지하는 페이지 수(1페이지 = 4KiB 4096B)
    uint64_t attribute;         //메모리 속성(읽기/쓰기 가능 여부 등)
};

#ifdef __cplusplus
enum class MemoryType{ // 메모리 유형을 정의(==EFI_MEMORY_TYPE 동일한 역할)
  kEfiReservedMemoryType,
  kEfiLoaderCode,
  kEfiLoaderData,
  kEfiBootServicesCode,
  kEfiBootServicesData,
  kEfiRuntimeServicesCode,
  kEfiRuntimeServicesData,
  kEfiConventionalMemory, // UEFI에서 일반적으로 사용할 수 있는 RAM 의미
  kEfiUnusableMemory,
  kEfiACPIReclaimMemory,
  kEfiACPIMemoryNVS,
  kEfiMemoryMappedIO,
  kEfiMemoryMappedIOPortSpace,
  kEfiPalCode,
  kEfiPersistentMemory,
  kEfiMaxMemoryType
};//UEFI메모리 타입을 C++스타일로 변경/enum class:이름 충돌 방지 목적

/* MemoryType과 uint32_t 비교 연산자
- MemoryType 열거형을 uint32_t와 비교할 수 있도록 연산자 오버로딩을 제공
- UEFI 메모리 타입은 uint32_t, C++ : enum class로 직접 비교 불가(암시적 변환 허용 x)
- 연산자 오버로딩을 이용해 해결 가능(코드 가독성 향상 및 타입 안전성 보장)
*/
inline bool operator==(uint32_t lhs, MemoryType rhs){
    return lhs == static_cast<uint32_t>(rhs);
}

inline bool operator==(MemoryType lhs, uint32_t rhs){
    return rhs == lhs;
}

/* 특정 메모리 유형이 사용 가능한지 확인하는 함수
- kEfiBootServicesCode, kEfiBootServicesData, kEfiConventionalMemory는 부팅 이후 OS가 사용할 수 있는 영역
- 안전한 메모리만 사용할 수 있도록 검사 가능
*/
inline bool IsAvailable(MemoryType memory_type) {
  return
    memory_type == MemoryType::kEfiBootServicesCode ||
    memory_type == MemoryType::kEfiBootServicesData ||
    memory_type == MemoryType::kEfiConventionalMemory;
}

const int kUEFIPageSize = 4096; //UEFI 페이지 크기 정의 (4096바이트)

#endif