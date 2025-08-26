#ifndef LCD_H
#define LCD_H

#include <avr/io.h>
#include <stdint.h>

/* --- LCD 포트 매핑 (D0~D7 = PB0~PB7, RS/RW/EN = PG0/PG1/PG2) --- */
#define LCD_DATA_PORT  PORTC
#define LCD_DATA_DDR   DDRC

#define LCD_CTRL_PORT  PORTG
#define LCD_CTRL_DDR   DDRG
#define LCD_RS 0   // PG0
#define LCD_RW 1   // PG1 (RW를 GND에 묶었으면 코드에서 안 써도 됨)
#define LCD_EN 2   // PG2

/* API */
void Portinit(void);
void LCD_init(void);
void LCD_Clear(void);
void LCD_pos(uint8_t row, uint8_t col);
void LCD_CHAR(char c);
void LCD_STR(const char *s);
/* 필요 시 직접 명령 */
void LCD_Comm(uint8_t cmd);

#endif /* LCD_H */
