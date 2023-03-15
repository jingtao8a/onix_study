#include "../include/bitmap.h"
#include "../include/assert.h"
#include "../include/string.h"
#include "../include/debug.h"

//构造位图
void bitmap_make(bitmap_t *map, char *bits, uint32 length, uint32 offset) {
    map->bits = bits;
    map->length = length;
    map->offset = offset;
}

//初始化位图
void bitmap_init(bitmap_t *map, char *bits, uint32 length, uint32 offset) {
    bitmap_make(map, bits, length, offset);
    memset(bits, 0, length);
}

//测试位图的某一位是否为1
bool bitmap_test(bitmap_t *map, uint32 index) {
    assert(index >= map->offset);

    uint32 idx = index - map->offset;
    uint32 bytes = idx / 8;
    uint32 bits = idx % 8;

    assert(bytes < map->length);
    return (map->bits[bytes] & (1 << bits)); 
}

//设置位图中某位的值
void bitmap_set(bitmap_t *map, uint32 index, bool value) {
    assert(value == true || value == false);
    assert(index >= map->offset);
    
    uint32 idx = index - map->offset;
    uint32 bytes = idx / 8;
    uint32 bits = idx % 8;

    assert(bytes < map->length);
    if (value) {
        map->bits[bytes] |= (1 << bits);//置1
    } else {
        map->bits[bytes] &= ~(1 << bits);//置0
    }
}

//从位图中得到连续的count位
int bitmap_scan(bitmap_t *map, uint32 count) {
    int start = EOF; //标记目标开始的位置
    uint32 bits_left = map->length * 8;
    uint32 counter = 0;
    for (uint32 next_bit = 0; next_bit < bits_left; ++next_bit) {
        if (!bitmap_test(map, map->offset + next_bit)) {
            counter++;
        } else {
            counter = 0;
        }
        if (counter == count) {
            start = next_bit - count + 1;
            break;
        }
    }
    if (start != EOF) {//找到
        for (uint32 next_bit = 0; next_bit < count; ++next_bit) {
            bitmap_set(map, map->offset + start + next_bit, true);//将找到的位全部置1
        }
        return start + map->offset;
    }
    //未找到
    return start;
}
