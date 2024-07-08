/* my_debug.c
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
*/
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
//
#include "pico/multicore.h"  // get_core_num()
#include "pico/stdlib.h"
#include "RP2040.h"
//
#include "FreeRTOS.h"
#include "ff_stdio.h"
#include "semphr.h"
#include "task.h"
//
#include "FreeRTOS_strerror.h"
#include "FreeRTOS_time.h"
#include "crash.h"
//
#include "my_debug.h"

static time_t start_time;

void mark_start_time() { start_time = FreeRTOS_time(NULL); }
time_t GLOBAL_uptime_seconds() { return FreeRTOS_time(NULL) - start_time; }

//static SemaphoreHandle_t xSemaphore;
//static BaseType_t printf_locked;
void lock_printf() {
    //static StaticSemaphore_t xMutexBuffer;
    //static bool once;
    //// bool __atomic_test_and_set (void *ptr, int memorder)
    //// This built-in function performs an atomic test-and-set operation on the byte at *ptr.
    //// The byte is set to some implementation defined nonzero “set” value
    //// and the return value is true if and only if the previous contents were “set”.
    //while (!once)
    //    if (!__atomic_test_and_set(&once, __ATOMIC_SEQ_CST))
    //        xSemaphore = xSemaphoreCreateMutexStatic(&xMutexBuffer);
    //if (xPortIsInsideInterrupt())
    //    printf_locked = xSemaphoreTakeFromISR(xSemaphore, NULL);
    //else
    //    printf_locked = xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(5000));
}
void unlock_printf() {
    //if (pdTRUE == printf_locked) {
    //    if (xPortIsInsideInterrupt())
    //        xSemaphoreGiveFromISR(xSemaphore, NULL);
    //    else
    //        xSemaphoreGive(xSemaphore);
    //}
}

/* Function Attribute ((weak))
The weak attribute causes a declaration of an external symbol to be emitted as a weak symbol
rather than a global. This is primarily useful in defining library functions that can be
overridden in user code, though it can also be used with non-function declarations. The
overriding symbol must have the same type as the weak symbol.
https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html

You can override these functions in your application to redirect "stdout"-type messages.
*/

/* Single string output callbacks */

void __attribute__((weak)) put_out_error_message(const char *s) { (void)s; }
void __attribute__((weak)) put_out_info_message(const char *s) { (void)s; }
void __attribute__((weak)) put_out_debug_message(const char *s) { (void)s; }

/* "printf"-style output callbacks */

#if defined(USE_PRINTF) && USE_PRINTF

int __attribute__((weak)) 
error_message_printf(const char *func, int line, 
        const char *fmt, ...) 
{
    printf("%s:%d: ", func, line);
    va_list args;
    va_start(args, fmt);
    lock_printf();
    int cw = vprintf(fmt, args);
    unlock_printf();
    va_end(args);
    // stdio_flush();
    fflush(stdout);
    return cw;
}
int __attribute__((weak)) error_message_printf_plain(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lock_printf();
    int cw = vprintf(fmt, args);
    unlock_printf();
    va_end(args);
    // stdio_flush();
    fflush(stdout);
    return cw;
}
int __attribute__((weak)) info_message_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lock_printf();
    int cw = vprintf(fmt, args);
    unlock_printf();
    va_end(args);
    return cw;
}
int __attribute__((weak)) 
debug_message_printf(const char *func, int line, 
		const char *fmt, ...) 
{
    lock_printf();
    printf("%s:%d: ", func, line);
    va_list args;
    va_start(args, fmt);
    int cw = vprintf(fmt, args);
    va_end(args);
    // stdio_flush();
    fflush(stdout);
    unlock_printf();
    return cw;
}

#else

/* These will truncate at 256 bytes. You can tell by checking the return code. */

int __attribute__((weak)) 
error_message_printf(const char *func, int line, 
        const char *fmt, ...) 
{
    char buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int cw = vsnprintf(buf, sizeof(buf), fmt, args);
    put_out_error_message(buf);
    va_end(args);
    return cw;
}
int __attribute__((weak)) error_message_printf_plain(const char *fmt, ...) {
    char buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int cw = vsnprintf(buf, sizeof(buf), fmt, args);
    put_out_info_message(buf);
    va_end(args);
    return cw;
}
int __attribute__((weak)) info_message_printf(const char *fmt, ...) {
    char buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int cw = vsnprintf(buf, sizeof(buf), fmt, args);
    put_out_info_message(buf);
    va_end(args);
    return cw;
}
int __attribute__((weak)) 
debug_message_printf(const char *func, int line, 
        const char *fmt, ...) 
{
    char buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int cw = vsnprintf(buf, sizeof(buf), fmt, args);
    put_out_debug_message(buf);
    va_end(args);
    return cw;
}

#endif

int task_printf(const char *pcFormat, ...) {
    char pcBuffer[256] = {0};
    va_list xArgs;
    va_start(xArgs, pcFormat);
    int cw = vsnprintf(pcBuffer, sizeof(pcBuffer), pcFormat, xArgs);
    va_end(xArgs);
    // if (xPortIsInsideInterrupt()) {
    // if (xPortInIsrContext() {
    if (portCHECK_IF_IN_ISR()) {
        printf("%s", pcBuffer);
    } else {
        lock_printf();
        printf("core%u: %s: %s", get_core_num(), pcTaskGetName(NULL), pcBuffer);
        fflush(stdout);
        unlock_printf();
    }
    return cw;
}

int stdio_fail(const char *const fn, const char *const str) {
    int error = stdioGET_ERRNO();
    FF_PRINTF("%s: %s: %s: %s (%d)\n", pcTaskGetName(NULL), fn, str, FreeRTOS_strerror(error),
              error);
    return error;
}

int ff_stdio_fail(const char *const func, char const *const str, char const *const filename) {
    int error = stdioGET_ERRNO();
    FF_PRINTF("%s: %s: %s(%s): %s (%d)\n", pcTaskGetName(NULL), func, str, filename,
              FreeRTOS_strerror(error), error);
    return error;
}

void my_assert_func(const char *file, int line, const char *func, const char *pred) {
    error_message_printf_plain(
        "%s: assertion \"%s\" failed: file \"%s\", line %d, function: %s\n",
        pcTaskGetName(NULL), pred, file, line, func);
    fflush(stdout);
    vTaskSuspendAll();
    __disable_irq(); /* Disable global interrupts. */
    capture_assert(file, line, func, pred);
}

//__attribute__((__noreturn__)) void __assert_func(const char *filename, int line,
//                                                 const char *assert_func, const char *expr) {
//    my_assert_func(filename, line, assert_func, expr);
//}

void assert_case_not_func(const char *file, int line, const char *func, int v) {
    char pred[128];
    snprintf(pred, sizeof pred, "case not %d", v);
    my_assert_func(file, line, func, pred);
}

void assert_case_is(const char *file, int line, const char *func, int v, int expected) {
    TRIG();  // DEBUG
    char pred[128];
    snprintf(pred, sizeof pred, "%d is %d", v, expected);
    my_assert_func(file, line, func, pred);
}

void dump8buf(char *buf, size_t buf_sz, uint8_t *pbytes, size_t nbytes) {
    int n = 0;
    for (size_t byte_ix = 0; byte_ix < nbytes; ++byte_ix) {
        for (size_t col = 0; col < 32 && byte_ix < nbytes; ++col, ++byte_ix) {
            n += snprintf(buf + n, buf_sz - n, "%02hhx ", pbytes[byte_ix]);
            configASSERT(0 < n && n < (int)buf_sz);
        }
        n += snprintf(buf + n, buf_sz - n, "\n");
        configASSERT(0 < n && n < (int)buf_sz);
    }
}
void hexdump_8(const char *s, const uint8_t *pbytes, size_t nbytes) {
    lock_printf();
    IMSG_PRINTF("\n%s: %s(%s, 0x%p, %u)\n", pcTaskGetName(NULL), __FUNCTION__, s, pbytes,
                nbytes);
    fflush(stdout);
    size_t col = 0;
    for (size_t byte_ix = 0; byte_ix < nbytes; ++byte_ix) {
        IMSG_PRINTF("%02hhx ", pbytes[byte_ix]);
        if (++col > 31) {
            IMSG_PRINTF("\n");
            col = 0;
        }
        fflush(stdout);
    }
    unlock_printf();
}
// nwords is size in WORDS!
void hexdump_32(const char *s, const uint32_t *pwords, size_t nwords) {
    lock_printf();
    IMSG_PRINTF("\n%s: %s(%s, 0x%p, %u)\n", pcTaskGetName(NULL), __FUNCTION__, s, pwords,
                nwords);
    fflush(stdout);
    size_t col = 0;
    for (size_t word_ix = 0; word_ix < nwords; ++word_ix) {
        IMSG_PRINTF("%08lx ", pwords[word_ix]);
        if (++col > 7) {
            IMSG_PRINTF("\n");
            col = 0;
        }
        fflush(stdout);
    }
    unlock_printf();
}
// nwords is size in bytes
bool compare_buffers_8(const char *s0, const uint8_t *pbytes0, const char *s1,
                       const uint8_t *pbytes1, const size_t nbytes) {
    /* Verify the data. */
    if (0 != memcmp(pbytes0, pbytes1, nbytes)) {
        hexdump_8(s0, pbytes0, nbytes);
        hexdump_8(s1, pbytes1, nbytes);
        return false;
    }
    return true;
}
// nwords is size in WORDS!
bool compare_buffers_32(const char *s0, const uint32_t *pwords0, const char *s1,
                        const uint32_t *pwords1, const size_t nwords) {
    /* Verify the data. */
    if (0 != memcmp(pwords0, pwords1, nwords * sizeof(uint32_t))) {
        hexdump_32(s0, pwords0, nwords);
        hexdump_32(s1, pwords1, nwords);
        return false;
    }
    return true;
}
/* [] END OF FILE */
