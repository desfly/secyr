#include "homeguard/crc32.hpp"
namespace hg {
uint32_t crc32(std::span<const std::byte> data) { uint32_t crc=0xFFFFFFFFU; for (auto b:data) { crc^=static_cast<uint8_t>(b); for(int i=0;i<8;++i) crc=(crc>>1)^((crc&1U)?0xEDB88320U:0U); } return ~crc; }
}
