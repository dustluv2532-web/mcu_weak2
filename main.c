#define F_CPU 14745600UL
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <avr/eeprom.h>
#include "lcd2.h"

/* ================= Keypad ================= */
static inline void keypad_init(void){
	DDRD  = 0x70;   // PD6..PD4=컬럼 출력, PD3..PD0=행 입력
	PORTD = 0x00;   // 풀업 미사용
}
static inline void col_select(uint8_t col){
	PORTD &= ~(0x70);
	PORTD |= (1 << (4 + col));
}
static char keypad_getkey_once(void){
	static const char keymap[4][3] = {
		{'1','2','3'}, {'4','5','6'}, {'7','8','9'}, {'*','0','#'}
	};
	for(uint8_t c=0;c<3;c++){
		col_select(c); _delay_us(5);
		uint8_t rows = PIND & 0x0F;
		for(uint8_t r=0;r<4;r++){
			uint8_t m = (1<<r);
			if(rows & m){
				_delay_ms(10);
				if(PIND & m){
					while(PIND & m){ _delay_ms(1); }
					return keymap[r][c];
				}
			}
		}
	}
	return 0;
}

/* ================= LED ================= */
#define LED_PORT PORTA
#define LED_DDR  DDRA
static inline void leds_init(void){ LED_DDR=0xFF; LED_PORT=0x00; }
static inline void leds_all_on(void){ LED_PORT=0xFF; }
static inline void leds_all_off(void){ LED_PORT=0x00; }
static void leds_blink_times(uint8_t n){
	for(uint8_t i=0;i<n;i++){ LED_PORT=0xFF; _delay_ms(150); LED_PORT=0x00; _delay_ms(150); }
}

/* ================= UART ================= */
#define LOG_UART 1   // 0=무출력(시연), 1=로그 출력(디버그)
static void uart_init(void){
	const uint16_t ubrr = 95;               // 14.7456MHz → 9600bps
	UBRR0H=(uint8_t)(ubrr>>8); UBRR0L=(uint8_t)(ubrr&0xFF);
	UCSR0A=0x00;
	UCSR0B=(1<<TXEN0);                      // USART0 TX enable
	UCSR0C=(1<<UCSZ01)|(1<<UCSZ00);         // 8N1
	/* ★ ATmega128A: USART0는 PORTE 사용 */
	DDRE  |= (1<<PE1);                      // PE1=TXD0 출력
	DDRE  &= ~(1<<PE0);                     // PE0=RXD0 입력
	PORTE |= (1<<PE0);                      // (옵션) RX 풀업
}
static inline void uart_print(const char *s){
	#if LOG_UART
	while(*s){ while(!(UCSR0A&(1<<UDRE0))); UDR0=*s++; }
	#endif
}
static inline void uart_println(const char *s){
	#if LOG_UART
	uart_print(s);
	while(!(UCSR0A&(1<<UDRE0))); UDR0='\r';
	while(!(UCSR0A&(1<<UDRE0))); UDR0='\n';
	#endif
}
/* ★ 해시를 직접 16진수로 송신(터미널 호환성↑) */
static void uart_print_hex32(uint32_t v){
	#if LOG_UART
	const char *hex = "0123456789ABCDEF";
	// "0x"
	while(!(UCSR0A&(1<<UDRE0))); UDR0='0';
	while(!(UCSR0A&(1<<UDRE0))); UDR0='x';
	for(int i=7;i>=0;i--){
		uint8_t nib = (v >> (i*4)) & 0xF;
		while(!(UCSR0A&(1<<UDRE0))); UDR0 = hex[nib];
	}
	while(!(UCSR0A&(1<<UDRE0))); UDR0='\r';
	while(!(UCSR0A&(1<<UDRE0))); UDR0='\n';
	#endif
}

/* ================= PIN 해시 ================= */
static uint32_t fnv1a32(const char *s, uint8_t len){
	uint32_t h = 0x811C9DC5UL;
	for(uint8_t i=0;i<len;i++){ h ^= (uint8_t)s[i]; h *= 0x01000193UL; }
	return h;
}

/* EEPROM: 기본 PIN "0258"의 해시 저장 */
uint32_t EEMEM EE_PIN_HASH;
static void ensure_pin_hash_initialized(void){
	eeprom_update_dword(&EE_PIN_HASH, 0xE175482AUL); // fnv1a32("0258")
}
static bool check_pin_secure(const char *typed, uint8_t len){
	if(len!=4) return false;
	uint32_t stored = eeprom_read_dword(&EE_PIN_HASH);
	uint32_t calc   = fnv1a32(typed, 4);
	return (stored ^ calc) == 0;
}

/* ================= 보조 함수/UI ================= */
static void secure_memzero(volatile char *p, uint8_t n){ while(n--) *p++=0; }
static inline void ui_ready(void){
	LCD_pos(0,0); LCD_STR("PIN: "); LCD_pos(0,5); LCD_STR("    ");
	LCD_pos(1,0); LCD_STR("                ");
}
/* 입력 마스킹 */
static inline void ui_mask(uint8_t len){
	LCD_pos(0,5);
	for(uint8_t i=0;i<4;i++) LCD_CHAR((i<len)?'*':' ');
}
/* 실패 3회 → 10초 lockout(두 자리 항상 재출력) */
static void lockout_10s(void){
	LCD_pos(1,0); LCD_STR("LOCK ");
	for (int8_t s = 10; s > 0; --s) {
		LCD_pos(1,6);
		char tens = (s >= 10) ? ('0' + (s / 10)) : ' ';
		char ones = '0' + (s % 10);
		LCD_CHAR(tens); LCD_CHAR(ones);
		leds_blink_times(1);
		_delay_ms(900);
	}
	LCD_pos(1,0); LCD_STR("                ");
}

/* ================= main ================= */
int main(void){
	Portinit(); LCD_init();
	keypad_init(); leds_init(); uart_init();

	#if LOG_UART
	uart_println("=== UART0 READY @9600 on PE1(TX) ===");
	uart_println("=== SECURE MODE: PIN_HASH only (no plaintext) ===");
	#endif

	ensure_pin_hash_initialized(); // PIN=0258 해시 저장

	ui_ready();
	char buf[5]={0}; uint8_t idx=0, fail_cnt=0;

	while(1){
		char k = keypad_getkey_once();
		if(!k){ _delay_ms(3); continue; }

		if(k=='#'){ // 제출(4자 아니어도 엔터)
			buf[idx]='\0';
			uint32_t calc = fnv1a32(buf, idx);        // 입력 PIN 해시(길이 기반)
			bool ok = check_pin_secure(buf, idx);

			#if LOG_UART
			uart_print(ok ? "[AUTH] OK " : "[AUTH] FAIL ");
			uart_print("LEN=");
			char l = '0' + (idx % 10);
			while(!(UCSR0A&(1<<UDRE0))); UDR0=l;
			while(!(UCSR0A&(1<<UDRE0))); UDR0=' ';
			uart_print("PIN_HASH=");
			uart_print_hex32(calc);                   // 해시만 출력
			#endif
			if(ok){ leds_all_on(); LCD_pos(1,0); LCD_STR("OK              "); }
			else  { leds_blink_times(6); leds_all_off(); LCD_pos(1,0); LCD_STR("FAIL            "); fail_cnt++; }

			/* 화면의 마스킹은 유지 */
			uint8_t keep = idx;
			secure_memzero(buf, sizeof(buf)); idx=0;
			ui_mask(keep);                            // 별표 개수 유지

			_delay_ms(300);
			if(fail_cnt>=3){ lockout_10s(); fail_cnt=0; ui_ready(); }
			continue;
		}

		if(k=='*'){ // 백스페이스
			if(idx>0){ idx--; buf[idx]='\0'; }
			ui_mask(idx);
			continue;
		}

		if(k>='0' && k<='9' && idx<4){
			if(idx==0){ ui_mask(0); }                // 새 입력 시작 시 클리어
			buf[idx++]=k;
			ui_mask(idx);

			if(idx==4){ // 자동 제출
				uint32_t calc = fnv1a32(buf, 4);
				bool ok = check_pin_secure(buf,4);

				#if LOG_UART
				uart_print(ok ? "[AUTH] OK " : "[AUTH] FAIL ");
				uart_print("LEN=4 ");
				uart_print("PIN_HASH=");
				uart_print_hex32(calc);
				#endif
				if(ok){ leds_all_on(); LCD_pos(1,0); LCD_STR("OK              "); }
				else  { leds_blink_times(6); leds_all_off(); LCD_pos(1,0); LCD_STR("FAIL            "); fail_cnt++; }

				secure_memzero(buf, sizeof(buf)); idx=0;
				ui_mask(4);                           // **** 유지7

				_delay_ms(300);
				if(fail_cnt>=3){ lockout_10s(); fail_cnt=0; ui_ready(); }
			}
		}
	}
}
