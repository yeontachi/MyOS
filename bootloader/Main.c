#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>

struct MemoryMap{
  UINTN buffer_size;
  VOID* buffer;
  UINTN map_size;
  UINTN map_key;
  UINTN descriptor_size;
  UINT32 descriptor_version;
};

EFI_STATUS GetMemoryMap(struct MemoryMap* map){
    if(map->buffer == NULL){ // 메모리 맵을 저장할 버퍼가 할당되지 않았다면 에러 반환
        return EFI_BUFFER_TOO_SMALL;
    }

    map->map_size = map->buffer_size;//충분한 크기의 버퍼를 미리 설정
    return gBS->GetMemoryMap( //gBS(Global Boot Services), 현재 시스템의 메모리 맵 정보를 반환
        &map->map_size,
        (EFI_MEMORY_DESCRIPTOR*)map->buffer,
        &map->map_size,
        &map->descriptor_size,
        &map->descriptor_version);
}