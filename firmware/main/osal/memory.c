#include "freertos/FreeRTOS.h"


/**
 * @brief Allocate memory from heap.
 *
 * This API should always allocate memory successfully. Otherwise we will print debug message and abort()
 *
 * @param size The size that you want to alloc
 * @return A pointer of memory
 */
void* osal_mem_malloc(size_t size)
{
  void* p = pvPortMalloc(size);
  configASSERT(p != NULL);
  return p;
}

/**
 * @brief Release memory.
 *
 * @param ptr The pointer of memory
 */
void osal_mem_free(void* ptr)
{
  if (ptr != NULL)
  {
    vPortFree(ptr);
  }
}

