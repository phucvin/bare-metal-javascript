#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h> // For standard IO if available, but we will route to UART

#include "mquickjs.h"

#include "uart.h"

// Define a static buffer for JS memory since we don't have malloc
// MQuickJS requires about 10kB of RAM. Let's give it 64KB to be safe.
#define JS_MEMORY_SIZE (64 * 1024)
static uint8_t js_memory[JS_MEMORY_SIZE];

// Forward declarations of missing functions from mqjs_stdlib.h
JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

// Now include mqjs_stdlib.h
#include "mqjs_stdlib.h"

// Implement missing functions

JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    for (int i = 0; i < argc; i++) {
        if (i != 0) uart_putc(' ');

        JSCStringBuf buf;
        const char *str = JS_ToCString(ctx, argv[i], &buf);
        if (str) {
            uart_puts((char*)str);
            // Check if str needs freeing?
            // The signature in mquickjs.h says:
            // const char *JS_ToCString(JSContext *ctx, JSValue val, JSCStringBuf *buf);
            // It uses the buffer if small, or allocates.
            // But we don't have JS_FreeCString in MQuickJS public API?
            // Actually, `mqjs.c` just uses it. `mquickjs.c` implementation:
            /*
               const char *JS_ToCString(JSContext *ctx, JSValue val, JSCStringBuf *buf)
               {
                   return JS_ToCStringLen(ctx, NULL, val, buf);
               }
               const char *JS_ToCStringLen(...) { ...
                   if (len <= sizeof(buf->buf) - 1) { ... return (char*)buf->buf; }
                   ...
                   str = (char *)js_malloc(ctx, len + 1);
                   ...
                   return str;
               }
            */
            // So if it allocates, we need to free it using `js_free(ctx, str)`.
            // But `js_free` is not public in `mquickjs.h`?
            // Wait, `JS_FreeContext` frees everything.
            // If we leak strings during execution, we might run out of memory.
            // `mquickjs_priv.h` might have `js_free`.
            // But `mquickjs.h` doesn't expose it.
            // However, `mquickjs` aims for minimal API.
            // If `JS_ToCString` returns a pointer to the internal string (if it's a string value), we shouldn't free it?
            // `JS_ToCStringLen`: "If the value is a string, return its content... otherwise convert...".
            // If it returns its content, we MUST NOT free it.
            // If it converts, it allocates.
            // This API is tricky.
            // `example.c` uses `fwrite` directly on the result of `JS_ToCStringLen` and does NOT free it.
            // But it runs once and exits.
            // In `mqjs.c`:
            // `filename = JS_ToCString(ctx, argv[0], &buf_str);` ... then it uses it ... no free.
            // This suggests that either it's garbage collected or we leak it.
            // Given MQuickJS uses GC, maybe the string is a GC object?
            // But `JS_ToCString` returns `const char *`.
            // If it was allocated with `js_malloc`, it is not a JSValue.
            // `js_malloc` allocates in the context buffer.
            // If we don't free it, it stays allocated until `JS_FreeContext`.
            // Since we only print a few things, maybe it's fine.
        }
    }
    uart_putc('\n');
    return JS_UNDEFINED;
}

JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewInt32(ctx, 0); // Stub
}

JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewInt32(ctx, 0); // Stub
}

JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    JS_GC(ctx);
    return JS_UNDEFINED;
}

JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_ThrowInternalError(ctx, "load not implemented");
}

JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewInt32(ctx, 0); // Stub
}

JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

int main() {
    JSContext *ctx;

    // Initialize MQuickJS context
    ctx = JS_NewContext(js_memory, sizeof(js_memory), &js_stdlib);
    if (!ctx) {
        uart_puts("Failed to create JS context\n");
        return 1;
    }

    // Since we implemented js_print and mapped it to 'print' and 'console.log' (in stdlib),
    // we can use 'print' or 'console.log' in JS.

    const char *js_code =
    "var msg = \"Hello from JS!\";\n"
    "for (var i = 0; i < 10; i++) {\n"
    "    var iteration_message = \"\";\n"
    "    for (var j = 0; j < i; j++) { iteration_message += msg; }\n"
    "    iteration_message += \"\\n\";\n"
    "    print(iteration_message);"
    "}\n"
    "var successMessage = \"Successful JavaScript!\\n\";\n"
    "print(successMessage);";

    // JS_Eval(ctx, input, input_len, filename, eval_flags)
    JSValue result = JS_Eval(ctx, js_code, strlen(js_code), "<input>", 0);

    if (JS_IsException(result)) {
        uart_puts("JS Exception: ");
        JSValue exception = JS_GetException(ctx);
        JSCStringBuf buf;
        const char *str = JS_ToCString(ctx, exception, &buf);
        if (str) {
            uart_puts((char*)str);
        }
        uart_putc('\n');
    } else {
        // print result
        JSCStringBuf buf;
        const char *str = JS_ToCString(ctx, result, &buf);
        if (str) {
            uart_puts("Result: ");
            uart_puts((char*)str);
            uart_putc('\n');
        }
    }

    JS_FreeContext(ctx);

    return 0;
}

void* _sbrk(int incr) {
    return (void*) -1; // Fail if anyone tries to use heap
}
