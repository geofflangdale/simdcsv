#include "io_util.h"
#include "mem_util.h"
#include <cstring>
#include <cstdlib>

uint8_t * allocate_padded_buffer(size_t length, size_t padding) {
    // we could do a simple malloc
    //return (char *) malloc(length + padding);
    // However, we might as well align to cache lines...
    size_t totalpaddedlength = length + padding;
    uint8_t * padded_buffer = (uint8_t *) aligned_malloc(64, totalpaddedlength);
    return padded_buffer;
}

std::basic_string_view<uint8_t> get_corpus(const std::string& filename, size_t padding) {
  std::FILE *fp = std::fopen(filename.c_str(), "rb");
  if (fp != nullptr) {
    std::fseek(fp, 0, SEEK_END);
    size_t len = std::ftell(fp);
    uint8_t * buf = allocate_padded_buffer(len, padding);
    if(buf == nullptr) {
      std::fclose(fp);
      throw  std::runtime_error("could not allocate memory");
    }
    std::rewind(fp);
    size_t readb = std::fread(buf, 1, len, fp);
    std::fclose(fp);
    if(readb != len) {
      aligned_free(buf);
      throw  std::runtime_error("could not read the data");
    }
    return std::basic_string_view<uint8_t>(buf, len+padding);
  }
  throw  std::runtime_error("could not load corpus");
}
