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

const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
  switch (type) {
    case EfiReservedMemoryType: return L"EfiReservedMemoryType";
    case EfiLoaderCode: return L"EfiLoaderCode";
    case EfiLoaderData: return L"EfiLoaderData";
    case EfiBootServicesCode: return L"EfiBootServicesCode";
    case EfiBootServicesData: return L"EfiBootServicesData";
    case EfiRuntimeServicesCode: return L"EfiRuntimeServicesCode";
    case EfiRuntimeServicesData: return L"EfiRuntimeServicesData";
    case EfiConventionalMemory: return L"EfiConventionalMemory";
    case EfiUnusableMemory: return L"EfiUnusableMemory";
    case EfiACPIReclaimMemory: return L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS: return L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO: return L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace: return L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode: return L"EfiPalCode";
    case EfiPersistentMemory: return L"EfiPersistentMemory";
    case EfiMaxMemoryType: return L"EfiMaxMemoryType";
    default: return L"InvalidMemoryType";
  }
}

//map 구조체에 저장된 메모리 맵을 file에 기록하는 함수
//EFI_FILE_PROTOCOL* file : UEFI 파일 시스템에 접근할 수 있는 포인터
EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file){
  //CSV 파일의 헤더 저장
  CHAR8 buf[256]; //ASCII 문자열 버퍼 (한 줄의 데이터를 저장)
  UINTN len;      //데이터 길이를 저장할 변수

  CHAR8* header = "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
  len = AsciiStrLen(header);
  file->Write(file, &len, header);  //헤더 라인을 출력(헤더 라인을 기록해 두면 CSV파일을 열었을 때 열의 의미를 알기 쉽다)

  Print(L"map->buffer = %08lx, map->map_size = %08lx\n", map->buffer, map->map_size);// 메모리 맵이 저장된 버퍼의 시작 주소와 크기를 출력하여 디버깅용 로그로 활용

  EFI_PHYSICAL_ADDRESS iter;      //메모리 맵의 각 요소 어드레스를 나타냄
  int i;                          //메모리 맵의 행 번호를 나타내는 카운터
  /*
  - iter의 초기화는 메모리 맵의 시작 즉 첫 번째 메모리 디스크립터를 가리킨다. 갱신에 따라 인접한 메모리 디스크립터를 차례차레 가리킴
  - 메모리 맵을 반복하면서 EFI_MEMORY_DESCRIPTOR를 하나씩 읽음
  - iter -> 현재 읽고 있는 메모리 맵의 주소
  - map->descriptor_size -> 각 EFI_MEMORY_DESCRIPTOR 크기(반복할 때 사용)
  */
  for(iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i=0; iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size; iter += map->descriptor_size, i++){
    EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;

    //AsciiPrint()를 이용해 각 메모리 블록을 CSV 형식 문자열로 변환
    len = AsciiPrint(buf, sizeof(buf), 
      "%u, %x, %-ls, %08lx, %lx, %lx\n",
      i, desc->Type, GetMemoryTypeUnicode(desc->Type),
      desc->PhysicalStart, desc->NumberOfPages,
      desc->Attribute & 0xffffflu);
    file->Write(file, &len, buf);//변환된 문자열을 파일에 기록하여 메모리 맵 정보를 memmap 파일에 저장
  }

  return EFI_SUCCESS;// 파일 저장이 성공적으로 완료되면 EFI_SUCCESS 반환
}

//EFI 파일 시스템에서 루트 디렉토리를 가져오는 함수
EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root){
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image; //현재 실행 중인 UEFI애플리케이션 정보를 담고 있음
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;     //현재 실행 중인 EFI 애플리케이션이 저장된 디스크에서 파일 시스템(fs) 핸들을 가져옴

  gBS->OpenProtocol(
    image_handle,
    &gEfiLoadedImageProtocolGuid,
    (VOID**)&loaded_image,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  gBS->OpenProtocol(
    loaded_image->DeviceHandle,
    &gEfiSimpleFileSystemProtocolGuid,
    (VOID**)&fs,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  fs->OpenVolume(fs, root); //루트 디렉토리 열기(root)

  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table){
  Print(L"Hello, World!\n");//시작 메세지

  //메모리 맵 저장을 위한 버퍼 준비
  CHAR8 memmap_buf[4096 * 4]; //메모리 맵을 저장할 버퍼를 선언
  struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
  GetMemoryMap(&memmap); //메모리 맵 정보 획득

  //루트 디렉토리 열기
  EFI_FILE_PROTOCOL* root_dir;
  OpenRootDir(image_handle, &root_dir); //UEFI 파일 시스템 루트 디렉토리 핸들을 가져옴

  //memmap 파일 생성
  EFI_FILE_PROTOCOL* memmap_file;
  root_dir->Open(       // 파일 시스템 루트 디렉토리에 memmap 파일 생성
    root_dir, &memmap_file, L"\\memmap",
    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  
  //메모리 맵을 memmap 파일에 저장
  SaveMemoryMap(&memmap, memmap_file);
  memmap_file->Close(memmap_file);//저장 후 닫음

  //실행 완료 메세지 및 무한 루프
  Print(L"All done\n");
  while(1);
  return EFI_SUCCESS;
}