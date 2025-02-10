#include <cstdint>

extern "C" void KernelMain(uint64_t frame_buffer_base, uint64_t frame_buffer_size){//부트로더로부터 호출된 함수 : 엔트리 포인트
    uint8_t* frame_buffer = reinterpret_cast<uint8_t*>(frame_buffer_base);
    for(uint64_t i = 0; i < frame_buffer_size; ++i){
        frame_buffer[i] = i % 256;
    }
    while(1) __asm__("hlt");
}
// extern"C": C언어 형식으로 함수를 정의함을 의미
// __asm__() : 인라인 어셈블러를 위한 기법, C언어 프로그램에서 어셈블리 언어 명령을 포함하고 싶을 때 사용
