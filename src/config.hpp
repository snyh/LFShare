#ifndef _CONFIG_HPP_
#define _CONFIG_HPP_
#include <cstdint>

/// 单个文件块最大尺寸, 注意UDP包有64K限制,因此这里大小最好不要超过60K.
const int CHUNK_SIZE = 60000;

/// UDP命令通道的端口
const int UDP_CMD_PORT = 18068;

/// UDP数据通道的端口
const int UDP_DATA_PORT = 18069;

/// BILL bits的类型
typedef uint32_t BlockType;
/// BILL bits 的大小
const int BLOCK_LEN = sizeof(BlockType)*8;

#ifdef WIN
const int INTERVAL_INIT = 0;
#else
const int INTERVAL_INIT = 5;
#endif

#endif
