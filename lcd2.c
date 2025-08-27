#define F_CPU 14745600UL
#include "lcd2.h"
#include <util/delay.h>

/* EN 펄스: 데이터/명령을 버스에 올린 뒤 호출 */
static inline void lcd_pulse_en(void){
	LCD_CTRL_PORT |=  (1 << LCD_EN);
	_delay_us(1);
	LCD_CTRL_PORT &= ~(1 << LCD_EN);
	_delay_us(50);
}

static inline void lcd_write_cmd(uint8_t cmd){
	LCD_CTRL_PORT &= ~(1 << LCD_RS);   // RS=0 (명령)
	LCD_CTRL_PORT &= ~(1 << LCD_RW);   // RW=0 (쓰기)
	LCD_DATA_PORT  = cmd;              // 데이터버스에 명령
	_delay_us(1);
	lcd_pulse_en();
}

static inline void lcd_write_data(uint8_t data){
	LCD_CTRL_PORT |=  (1 << LCD_RS);   // RS=1 (데이터)
	LCD_CTRL_PORT &= ~(1 << LCD_RW);   // RW=0 (쓰기)
	LCD_DATA_PORT  = data;             // 데이터버스에 데이터
	_delay_us(1);
	lcd_pulse_en();
}

/* 공개 API */
void Portinit(void){
	/* 데이터버스: PB0~PB7 출력 */
	LCD_DATA_DDR = 0xFF;

	/* 제어핀: PG0/PG1/PG2 출력 (RW를 안 쓰면 아래 RW 관련 라인 제거 가능) */
	LCD_CTRL_DDR |= (1 << LCD_RS) | (1 << LCD_RW) | (1 << LCD_EN);

	/* 초기 상태: RS=0, RW=0, EN=0 */
	LCD_CTRL_PORT &= ~((1 << LCD_RS) | (1 << LCD_RW) | (1 << LCD_EN));
}

void LCD_init(void){
	_delay_ms(40);                 // 전원 안정 대기

	/* 표준 8bit 초기화 시퀀스 (전원 직후 호환성↑) */
	lcd_write_cmd(0x30); _delay_ms(5);
	lcd_write_cmd(0x30); _delay_us(150);
	lcd_write_cmd(0x30); _delay_us(150);

	/* Function set: 8bit, 2 line, 5x8 */
	lcd_write_cmd(0x38); _delay_us(50);

	/* Display OFF, Clear, Entry Mode, Display ON */
	lcd_write_cmd(0x08); _delay_us(50);
	lcd_write_cmd(0x01); _delay_ms(2);
	lcd_write_cmd(0x06); _delay_us(50);
	lcd_write_cmd(0x0C); _delay_us(50);  // Cursor OFF
}

void LCD_Clear(void){
	lcd_write_cmd(0x01);
	_delay_ms(2);
}

void LCD_pos(uint8_t row, uint8_t col){
	uint8_t addr = (row ? 0x40 : 0x00) + col;  // 16x2 기준
	lcd_write_cmd(0x80 | addr);
}

void LCD_CHAR(char c){
	lcd_write_data((uint8_t)c);
}

void LCD_STR(const char *s){
	while (*s) LCD_CHAR(*s++);
}

/* 필요 시 노출 */
void LCD_Comm(uint8_t cmd){ lcd_write_cmd(cmd); }
