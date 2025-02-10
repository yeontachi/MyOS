#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Library/MemoryAllocationLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>
#include  <Guid/FileInfo.h>

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
        &map->map_key,
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
  EFI_STATUS status;
  CHAR8 buf[256]; //ASCII 문자열 버퍼 (한 줄의 데이터를 저장)
  UINTN len;      //데이터 길이를 저장할 변수

  CHAR8* header = "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
  len = AsciiStrLen(header);
  status = file->Write(file, &len, header);  //헤더 라인을 출력(헤더 라인을 기록해 두면 CSV파일을 열었을 때 열의 의미를 알기 쉽다)
  if(EFI_ERROR(status)){
    return status;
  }

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
    status = file->Write(file, &len, buf);//변환된 문자열을 파일에 기록하여 메모리 맵 정보를 memmap 파일에 저장
    if(EFI_ERROR(status)){
      return status;
    }
  }

  return EFI_SUCCESS;// 파일 저장이 성공적으로 완료되면 EFI_SUCCESS 반환
}

//EFI 파일 시스템에서 루트 디렉토리를 가져오는 함수
EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root){
  EFI_STATUS status;
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image; //현재 실행 중인 UEFI애플리케이션 정보를 담고 있음
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;     //현재 실행 중인 EFI 애플리케이션이 저장된 디스크에서 파일 시스템(fs) 핸들을 가져옴

  status = gBS->OpenProtocol(
    image_handle,
    &gEfiLoadedImageProtocolGuid,
    (VOID**)&loaded_image,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if(EFI_ERROR(status)){
    return status;
  }

  status = gBS->OpenProtocol(
    loaded_image->DeviceHandle,
    &gEfiSimpleFileSystemProtocolGuid,
    (VOID**)&fs,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if (EFI_ERROR(status)){
    return status;
  }

  return fs->OpenVolume(fs, root); //루트 디렉토리 열기(root)
}

EFI_STATUS OpenGOP(EFI_HANDLE image_handle, EFI_GRAPHICS_OUTPUT_PROTOCOL** gop){
  EFI_STATUS status;
  UINTN num_gop_handles = 0;
  EFI_HANDLE* gop_handles = NULL;

  status = gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiGraphicsOutputProtocolGuid,
    NULL,
    &num_gop_handles,
    &gop_handles);
  if (EFI_ERROR(status)){
    return status;
  }

  status = gBS->OpenProtocol(
    gop_handles[0],
    &gEfiGraphicsOutputProtocolGuid,
    (VOID**)gop,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if (EFI_ERROR(status)){
    return status;
  }

  FreePool(gop_handles);

  return EFI_SUCCESS;
}

const CHAR16* GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt){
  switch(fmt){
    case PixelRedGreenBlueReserved8BitPerColor:
      return L"PixelRedGreenBlueReserved8BitPerColor";
    case PixelBlueGreenRedReserved8BitPerColor:
      return L"PixelBlueGreenRedReserved8BitPerColor";
    case PixelBitMask:
      return L"PixelBitMask";
    case PixelBltOnly:
      return L"PixelBltOnly";
    case PixelFormatMax:
      return L"PixelFormatMax";
    default:
      return L"InvalidPixelFormat";
  }
}

void Halt(void){
  while(1) __asm__("hlt");
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table){
  EFI_STATUS status;

  Print(L"Hello, MyOS World!\n");//시작 메세지

  //메모리 맵 저장을 위한 버퍼 준비
  CHAR8 memmap_buf[4096 * 4]; //메모리 맵을 저장할 버퍼를 선언
  struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
  status = GetMemoryMap(&memmap); //메모리 맵 정보 획득
  if (EFI_ERROR(status)){
    Print(L"failed to get memory map: %r\n", status);
    Halt();
  }
  //루트 디렉토리 열기
  EFI_FILE_PROTOCOL* root_dir;
  status = OpenRootDir(image_handle, &root_dir); //UEFI 파일 시스템 루트 디렉토리 핸들을 가져옴
  if (EFI_ERROR(status)){
    Print(L"failed to open root directory: %r\n", status);
    Halt();
  }

  //memmap 파일 생성
  EFI_FILE_PROTOCOL* memmap_file;
  status = root_dir->Open(       // 파일 시스템 루트 디렉토리에 memmap 파일 생성
    root_dir, &memmap_file, L"\\memmap",
    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR(status)){
    Print(L"failed to open file '\\memmap': %r\n", status);
    Print(L"Ignored,\n");
  } else{
    status = SaveMemoryMap(&memmap, memmap_file);
    if(EFI_ERROR(status)){
      Print(L"failed to save memory map: %r\n", status);
      Halt();
    }
    status = memmap_file->Close(memmap_file);
    if(EFI_ERROR(status)){
      Print(L"failed to close memory map: %r\n", status);
      Halt();
    }
  }

  EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
  status = OpenGOP(image_handle, &gop);
  if(EFI_ERROR(status)){
    Print(L"failed to open GOP: %r\n", status);
    Halt();
  }
  Print(L"Resolution: %ux%u, Pixel Format: %s, %u pixels/line\n",
      gop->Mode->Info->HorizontalResolution,
      gop->Mode->Info->VerticalResolution,
      GetPixelFormatUnicode(gop->Mode->Info->PixelFormat),
      gop->Mode->Info->PixelsPerScanLine);
  Print(L"Frame Buffer: 0x%0lx - 0x%0lx, Size: %lu bytes\n",
      gop->Mode->FrameBufferBase,
      gop->Mode->FrameBufferBase + gop->Mode->FrameBufferSize,
      gop->Mode->FrameBufferSize);

  UINT8* frame_buffer = (UINT8*)gop->Mode->FrameBufferBase;
  for(UINTN i = 0; i < gop->Mode->FrameBufferSize; ++i){
    frame_buffer[i] = 255;
  }

  //####begin(read_kernel)####
  EFI_FILE_PROTOCOL* kernel_file;// 커널 파일을 가리키는 핸들 역할
  status = root_dir->Open(root_dir, &kernel_file, L"\\kernel.elf", EFI_FILE_MODE_READ, 0);//kernel.elf파일을 읽기 모드로 엶
  if(EFI_ERROR(status)){
    Print(L"failed to open file '\\kernel.elf': %r\n", status);
    Halt();
  }

  UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16)*12;  // 파일 정보를 저장할 버퍼 크기 정의
  UINT8 file_info_buffer[file_info_size];                            // 파일 정보를 저장할 버퍼 선언
  status = kernel_file->GetInfo(kernel_file, &gEfiFileInfoGuid, &file_info_size, file_info_buffer); //GetInfo를 호출해 kernel.elf 파일의 정보를 가져옴, gEfiFileInfoGuid : EFI_FILE_INFO를 요청하는 GUID
  if(EFI_ERROR(status)){
    Print(L"failed to get file information: %r\n", status);
    Halt();
  }

  EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer;
  UINTN kernel_file_size = file_info->FileSize;

  EFI_PHYSICAL_ADDRESS kernel_base_addr = 0x100000;                                                               //커널을 로드할 메모리 주소를 0ㅌ100000(1MB 위치)로 설정 //일반적으로 1MB 이상의 영역은 운영체제 로딩을 위한 공간으로사용됨
  status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, (kernel_file_size + 0xfff) / 0x1000, &kernel_base_addr);     //메모리를 할당하는 함수 kernel_base_addr에 메모리 할당, 메모리 타입으로 로더 데이터로 설정, 페이지 수 계산(4KB단위 메모리 할당, + 0xfff를 더해서 올림연산 수행), 할당된 메모리의 시작주소를 kernel_base_addr에 저장
  if(EFI_ERROR(status)){
    Print(L"failed to allocate pages: %r", status);
    Halt();
  }
  
  status = kernel_file->Read(kernel_file, &kernel_file_size, (VOID*)kernel_base_addr);                                     //kernel.elf 파일 내용을 읽어 메모리(kernel_base_addr)로 복사. kernel_file_size만큼 읽음
  if(EFI_ERROR(status)){
    Print(L"error: %r", status);
    Halt();
  }
  Print(L"Kernel: 0x%0lx (%lu bytes)\n", kernel_base_addr, kernel_file_size);                                     //커널이 로드된 주소와 크기 출력
  //####end(read_kernel)####

  //####begin(exit_bs)####
  status = gBS->ExitBootServices(image_handle, memmap.map_key);                     //ExitBootServices를 호출하면 UEFI의 모든 부트 서비스가 종료됨, 이후에는 UEFI 관련 API를 사용할 수 없음
  if(EFI_ERROR(status)){                                                            //실패하면 다시 호출
    //최신 메모리 맵 가져오기
    status = GetMemoryMap(&memmap);                                                 //최신 메모리 맵을 다시 가져오고 다시 호출 > 그래도 실패하면 무한 루프
    if(EFI_ERROR(status)){                                                          //실패 이유? : memmap.map_key가 변경되었을 가능성 존재...그래서 다시 최신 메모리맵을 가져온 후 다시 Exit호출
      Print(L"failed to get memory map: %r\n", status);
      Halt();
    }
    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    if(EFI_ERROR(status)){
      Print(L"Could not exit boot service: %r\n", status);
      Halt();
    }
  }
  //####end(exit_bs)####

  //####begin(call_kernel)####
  UINT64 entry_addr = *(UINT64*)(kernel_base_addr + 24);                           //커널 ELF파일에서 엔트리 포인트 주소를 가져옴  kernel_base_addr + 24는 ELF헤더의 e_entry필드(엔트리 포인트 주소)를 의미

  typedef void EntryPointType(UINT64, UINT64);                                               //EntryPointType은 반환값과 인자가 없는 함수 타입을 정의                           
  EntryPointType* entry_point = (EntryPointType*)entry_addr;                       //entry_addr를 함수 포인터(entry_point)로 캐스팅
  entry_point(gop->Mode->FrameBufferBase, gop->Mode->FrameBufferSize);                                                                   //entry_point()를 호출하여 커널을 실행  
  //####end(call_kernel)####

  //실행 완료 메세지 및 무한 루프
  Print(L"All done\n");
  
  while(1);
  return EFI_SUCCESS;
}