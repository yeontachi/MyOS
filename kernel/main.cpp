#include <cstdint>
#include <cstddef>

#include "frame_buffer_config.hpp"

struct PixelColor{
    uint8_t r, g, b;
};

int WritePixel(const FrameBufferConfig& config, int x, int y, const PixelColor& c){
    const int pixel_position = config.pixels_per_scan_line * y + x;
    if(config.pixel_format == kPixelRGBResv8BitPerColor){
        uint8_t* p = &config.frame_buffer[4 * pixel_position];
        p[0] = c.r;
        p[1] = c.g;
        p[2] = c.b;
    }
    else if(config.pixel_format == kPixelBGRResv8BitPerColor){
        uint8_t* p = &config.frame_buffer[4 * pixel_position];
        p[0] = c.b;
        p[1] = c.g;
        p[2] = c.r;
    }
    else{
        return -1;
    }
    return 0;
}

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config){//부트로더로부터 호출된 함수 : 엔트리 포인트
    for(int x = 0; x < frame_buffer_config.horizontal_resolution; ++x){
        for(int y = 0; y<frame_buffer_config.vertical_resolution; ++y){
            WritePixel(frame_buffer_config, x, y, {255, 255, 255});
        }
    }
    for(int x = 0; x < 200; ++x){
        for(int y = 0; y < 100; ++y){
            WritePixel(frame_buffer_config, 100 + x, 100 + y, {0, 255, 0});
        }
    }
    while(1) __asm__("hlt");
}
// extern"C": C언어 형식으로 함수를 정의함을 의미
// __asm__() : 인라인 어셈블러를 위한 기법, C언어 프로그램에서 어셈블리 언어 명령을 포함하고 싶을 때 사용
