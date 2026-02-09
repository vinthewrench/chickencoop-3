#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void uart_init(void);
int  uart_getc(void);
void uart_putc(char c);

#ifdef __cplusplus
}
#endif
