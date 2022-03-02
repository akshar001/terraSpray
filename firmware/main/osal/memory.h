#ifndef OSAL_MEM_H
#define OSAL_MEM_H


#include <stddef.h>

/**
 * @brief Allocate memory from heap.
 *
 * This API should always allocate memory successfully. Otherwise we will print debug message and abort()
 *
 * @param size The size that you want to alloc
 * @return A pointer of memory
 */
extern void* osal_mem_malloc(size_t size);

/**
 * @brief Release memory.
 *
 * @param ptr The pointer of memory
 */
extern void osal_mem_free(void* ptr);

#endif /* OSAL_MEM_H */
